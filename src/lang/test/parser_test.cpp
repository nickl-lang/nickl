#include "nkl/lang/parser.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>

#include <gtest/gtest.h>

#include "nk/ds/array.hpp"
#include "nk/mem/stack_allocator.hpp"
#include "nk/str/dynamic_string_builder.hpp"
#include "nk/str/string_builder.hpp"
#include "nk/utils/logger.h"
#include "nkl/core/test/ast_utils.hpp"
#include "nkl/lang/ast.hpp"
#include "nkl/lang/tokens.hpp"

namespace {

using namespace nk;
using namespace nkl;

LOG_USE_SCOPE(nkl::parser::test);

class parser : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_ast.init();

        id_init();

        m_sb.reserve(1000);
    }

    void TearDown() override {
        id_deinit();

        m_ast.deinit();

        m_arena.deinit();
        m_sb.deinit();
    }

protected:
    Token createToken(ETokenID id = t_eof, char const *text = "") {
        return Token{{text, std::strlen(text)}, 0, 0, 0, (uint8_t)id};
    }

    TokenRef mkt(ETokenID id = t_eof, char const *text = "") {
        Token *token = m_arena.alloc<Token>();
        *token = createToken(id, text);
        return token;
    }

    struct TokenDescr {
        ETokenID id;
        const char *text;

        TokenDescr(ETokenID id)
            : id{id}
            , text{s_token_text[id]} {
        }

        TokenDescr(ETokenID id, const char *text)
            : id{id}
            , text{text} {
        }
    };

    void test_ok(std::vector<TokenDescr> tokens, Node const &expected) {
        StackAllocator arena{};

        defer {
            m_parser.ast.deinit();
            arena.deinit();
        };

        LOG_INF("\n==================================================\nTest: %s", [&]() {
            for (auto const &token : tokens) {
                m_sb << token.text << " ";
            }
            auto str = m_sb.moveStr(arena);
            return str.data;
        }());
        tokens.emplace_back(t_eof, "");
        Array<Token> token_ar{};
        defer {
            token_ar.deinit();
        };
        for (auto const &token : tokens) {
            *token_ar.push() = createToken(token.id, token.text);
        }
        bool parse_ok = m_parser.parse(token_ar);
        EXPECT_TRUE(parse_ok);
        if (!parse_ok) {
            auto const err_msg = m_parser.err.moveStr(arena);
            LOG_ERR("Error message: %.*s", err_msg.size, err_msg.data);
        }

        bool trees_are_equal = test::ast_equal(m_parser.root, &expected);
        EXPECT_TRUE(trees_are_equal);
        if (!trees_are_equal) {
            m_sb << "\nActual tree: ";
            ast_inspect(m_parser.root, m_sb);
            m_sb << "\nExpected tree: ";
            ast_inspect(&expected, m_sb);

            auto const msg = m_sb.moveStr(arena);
            LOG_ERR("%.*s", msg.size, msg.data);
        }
    }

protected:
    StackAllocator m_arena{};

    DynamicStringBuilder m_sb{};
    LangAst m_ast{};
    Parser m_parser{m_sb};
};

} // namespace

TEST_F(parser, empty) {
    test_ok({}, m_ast.make_nop());
}

TEST_F(parser, nullary) {
    test_ok({t_break, t_semi}, m_ast.make_break());
    test_ok({t_continue, t_semi}, m_ast.make_continue());
    test_ok({t_false, t_semi}, m_ast.make_false());
    test_ok({t_true, t_semi}, m_ast.make_true());
    test_ok({t_i16, t_semi}, m_ast.make_i16());
    test_ok({t_i32, t_semi}, m_ast.make_i32());
    test_ok({t_i64, t_semi}, m_ast.make_i64());
    test_ok({t_i8, t_semi}, m_ast.make_i8());
    test_ok({t_u16, t_semi}, m_ast.make_u16());
    test_ok({t_u32, t_semi}, m_ast.make_u32());
    test_ok({t_u64, t_semi}, m_ast.make_u64());
    test_ok({t_u8, t_semi}, m_ast.make_u8());
    test_ok({t_void, t_semi}, m_ast.make_void());
    test_ok({t_f32, t_semi}, m_ast.make_f32());
    test_ok({t_f64, t_semi}, m_ast.make_f64());
}

TEST_F(parser, unary) {
    auto a = m_ast.make_id(mkt(t_id, "a"));

    TokenDescr t_a{t_id, "a"};

    test_ok({t_amper, t_a, t_semi}, m_ast.make_addr(a));
    test_ok({t_tilde, t_a, t_semi}, m_ast.make_compl(a));
    test_ok({t_aster, t_a, t_semi}, m_ast.make_deref(a));
    test_ok({t_exclam, t_a, t_semi}, m_ast.make_not(a));
    test_ok({t_return, t_a, t_semi}, m_ast.make_return(a));
    test_ok({t_minus, t_a, t_semi}, m_ast.make_uminus(a));
    test_ok({t_plus, t_a, t_semi}, m_ast.make_uplus(a));
}

// TEST_F(parser, binary) {
//     auto a = m_ast.make_id(mkt(t_id, "a"));
//     auto num = m_ast.make_numeric_int(mkt(t_int_const, "42"));

//     TokenDescr t_a{t_id, "a"};
//     TokenDescr t_num{t_int_const, "42"};

//     // clang-format off
//     test_ok({t_num, t_plus, t_num, t_semi},
//             m_ast.make_add(num, num));
//     test_ok({t_a, t_plus_eq, t_num, t_semi},
//             m_ast.make_add_assign(a, num));
//     test_ok({t_num, t_amper_2x, t_num, t_semi},
//             m_ast.make_and(num, num));
//     test_ok({t_a, t_amper_eq, t_num, t_semi},
//             m_ast.make_and_assign(a, num));
//     test_ok({t_a, t_eq, t_num, t_semi},
//             m_ast.make_assign(a, num));
//     test_ok({t_num, t_amper, t_num, t_semi},
//             m_ast.make_bitand(num, num));
//     test_ok({t_num, t_amper_2x_eq, t_num, t_semi},
//             m_ast.make_bitand_assign(num, num));
//     test_ok({t_num, t_bar, t_num, t_semi},
//             m_ast.make_bitor(num, num));
//     test_ok({t_num, t_bar_2x_eq, t_num, t_semi},
//             m_ast.make_bitor_assign(num, num));
//     test_ok({t_cast, t_par_l, t_f64, t_par_r, t_num, t_semi},
//             m_ast.make_cast(m_ast.make_f64(), num));
//     test_ok({t_a, t_colon_eq, t_num, t_semi},
//             m_ast.make_colon_assign(a, num));
//     test_ok({t_num, t_slash, t_num, t_semi},
//             m_ast.make_div(num, num));
//     test_ok({t_a, t_slash_eq, t_num, t_semi},
//             m_ast.make_div_assign(a, num));
//     test_ok({t_num, t_eq_2x, t_num, t_semi},
//             m_ast.make_eq(num, num));
//     test_ok({t_num, t_greater_eq, t_num, t_semi},
//             m_ast.make_ge(num, num));
//     test_ok({t_num, t_greater, t_num, t_semi},
//             m_ast.make_gt(num, num));
//     test_ok({t_a, t_bracket_l, t_num, t_bracket_r, t_semi},
//             m_ast.make_index(a, num));
//     test_ok({t_num, t_less_eq, t_num, t_semi},
//             m_ast.make_le(num, num));
//     test_ok({t_num, t_less_2x, t_num, t_semi},
//             m_ast.make_lsh(num, num));
//     test_ok({t_num, t_less_2x_eq, t_num, t_semi},
//             m_ast.make_lsh_assign(num, num));
//     test_ok({t_num, t_less, t_num, t_semi},
//             m_ast.make_lt(num, num));
//     test_ok({t_num, t_percent, t_num, t_semi},
//             m_ast.make_mod(num, num));
//     test_ok({t_a, t_percent_eq, t_num, t_semi},
//             m_ast.make_mod_assign(a, num));
//     test_ok({t_num, t_aster, t_num, t_semi},
//             m_ast.make_mul(num, num));
//     test_ok({t_a, t_aster_eq, t_num, t_semi},
//             m_ast.make_mul_assign(a, num));
//     test_ok({t_num, t_exclam_eq, t_num, t_semi},
//             m_ast.make_ne(num, num));
//     test_ok({t_num, t_bar_2x, t_num, t_semi},
//             m_ast.make_or(num, num));
//     test_ok({t_a, t_bar_eq, t_num, t_semi},
//             m_ast.make_or_assign(a, num));
//     test_ok({t_num, t_greater_2x, t_num, t_semi},
//             m_ast.make_rsh(num, num));
//     test_ok({t_a, t_greater_2x_eq, t_num, t_semi},
//             m_ast.make_rsh_assign(a, num));
//     test_ok({t_num, t_minus, t_num, t_semi},
//             m_ast.make_sub(num, num));
//     test_ok({t_a, t_minus_eq, t_num, t_semi},
//             m_ast.make_sub_assign(a, num));
//     test_ok({t_array_t, t_brace_l, t_f64, t_comma, t_num, t_brace_r, t_semi},
//             m_ast.make_array_type(m_ast.make_f64(), num));
//     test_ok({t_while, t_num, t_brace_l, t_a, t_semi, t_brace_r},
//             m_ast.make_while(num, m_ast.make_scope(a)));
//     test_ok({t_num, t_caret, t_num, t_semi},
//             m_ast.make_xor(num, num));
//     test_ok({t_a, t_caret_eq, t_num, t_semi},
//             m_ast.make_xor_assign(a, num));
//     // clang-format on
// }

// TEST_F(parser, ternary) {
//     auto a = m_ast.make_id(mkt(t_id, "a"));
//     auto b = m_ast.make_id(mkt(t_id, "b"));
//     auto num = m_ast.make_numeric_int(mkt(t_int_const, "42"));

//     TokenDescr t_a{t_id, "a"};
//     TokenDescr t_b{t_id, "b"};
//     TokenDescr t_num{t_int_const, "42"};

//     // clang-format off
//     test_ok({t_num, t_question, t_a, t_colon, t_b, t_semi},
//             m_ast.make_ternary(num, a, b));
//     test_ok({t_if, t_num, t_brace_l, t_a, t_semi, t_brace_r},
//             m_ast.make_if(num, m_ast.make_scope(a), {}));
//     test_ok({t_if, t_num, t_brace_l, t_a, t_semi, t_brace_r, t_else, t_brace_l, t_b, t_semi,
//     t_brace_r},
//             m_ast.make_if(num, m_ast.make_scope(a), m_ast.make_scope(b)));
//     // clang-format on
// }

// TEST_F(parser, other) {
//     auto id_a = mkt(t_id, "a");
//     auto a = m_ast.make_id(id_a);
//     auto id_b = mkt(t_id, "b");
//     auto b = m_ast.make_id(id_b);
//     auto id_A = mkt(t_id, "A");
//     auto id_plus = mkt(t_id, "plus");
//     auto num = m_ast.make_numeric_int(mkt(t_int_const, "42"));
//     auto i64 = m_ast.make_i64();
//     auto f64 = m_ast.make_f64();

//     TokenDescr t_a{t_id, "a"};
//     TokenDescr t_b{t_id, "b"};
//     TokenDescr t_A{t_id, "A"};
//     TokenDescr t_id_plus{t_id, "plus"};
//     TokenDescr t_num{t_int_const, "42"};

//     static constexpr const char *c_test_str = "hello";

//     Node ab_ar[] = {a, b};
//     Node i64f64_ar[] = {i64, f64};

//     // clang-format off
//     test_ok({t_bracket_l, t_a, t_comma, t_b, t_bracket_r, t_semi},
//             m_ast.make_array({ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])}));
//     test_ok({t_brace_l, t_a, t_semi, t_b, t_semi, t_brace_r, t_semi},
//             m_ast.make_scope(m_ast.make_block({ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])})));
//     test_ok({t_a, t_comma, t_b, t_semi},
//             m_ast.make_id_tuple({ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])}));
//     test_ok({t_tuple_t, t_brace_l, t_i64, t_comma, t_f64, t_brace_r, t_semi},
//             m_ast.make_tuple_type({i64f64_ar, sizeof(i64f64_ar) / sizeof(i64f64_ar[0])}));
//     test_ok({t_ptr_t, t_brace_l, t_ptr_t, t_brace_l, t_i8, t_brace_r, t_brace_r, t_semi},
//             m_ast.make_ptr_type(m_ast.make_ptr_type(m_ast.make_i8())));
//     // clang-format on

//     NamedNode name_ar[] = {{id_a, m_ast.push(f64)}, {id_b, m_ast.push(f64)}};

//     // clang-format off
//     test_ok({t_struct, t_A, t_brace_l, t_a, t_colon, t_f64, t_semi, t_b, t_colon, t_f64, t_semi,
//     t_brace_r},
//             m_ast.make_struct(id_A, {name_ar, sizeof(name_ar) / sizeof(name_ar[0])}));
//     test_ok({t_a, t_period, t_b, t_semi},
//             m_ast.make_member(a, id_b));
//     test_ok({{t_float_const, "3.14"}, t_semi},
//             m_ast.make_numeric_float(mkt(t_float_const, "3.14")));
//     test_ok({t_num, t_semi},
//             m_ast.make_numeric_int(mkt(t_int_const, "42")));
//     test_ok({t_a, t_semi},
//             m_ast.make_id(id_a));
//     test_ok({t_a, t_par_l, t_a, t_comma, t_b, t_par_r, t_semi},
//             m_ast.make_call(a, {ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])}));
//     test_ok({t_a, t_colon, t_f64, t_eq, t_num, t_semi},
//             m_ast.make_var_decl(id_a, f64, num));
//     test_ok({t_fn, t_id_plus, t_par_l, t_a, t_colon, t_f64, t_comma, t_b, t_colon, t_f64,
//     t_par_r, t_minus_greater, t_i64, t_brace_l, t_return, t_a, t_plus, t_b, t_semi, t_brace_r},
//             m_ast.make_fn( id_plus, {name_ar, sizeof(name_ar) / sizeof(name_ar[0])}, i64,
//             m_ast.make_scope(m_ast.make_return(m_ast.make_add(a, b)))));
//     test_ok({{t_str_const, c_test_str}, t_semi},
//             m_ast.make_string_literal(mkt(t_str_const, c_test_str)));
//     // test_ok({t_new, t_A, t_brace_l, t_a, t_colon, t_f64, t_comma, t_b, t_colon, t_f64,
//     t_brace_r, t_semi},
//     //         m_ast.make_struct_literal(A, {{id_a, f64}, {id_b, f64}}));
//     // clang-format on
// }
