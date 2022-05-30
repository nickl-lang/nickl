#include "nkl/core/parser.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/vm/vm.hpp"

namespace {

using namespace nk;
using namespace nkl;

LOG_USE_SCOPE(nkl::parser::test)

Token createToken(ETokenID id = t_eof, char const *text = "") {
    return Token{{text, std::strlen(text)}, 0, 0, 0, (uint8_t)id};
}

token_ref_t mkt(ETokenID id = t_eof, char const *text = "") {
    Token *token = _mctx.tmp_allocator->alloc<Token>();
    *token = createToken(id, text);
    return token;
}

class parser : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        vm::vm_init({});
        m_ast.init();
    }

    void TearDown() override {
        m_ast.deinit();
        vm::vm_deinit();
    }

protected:
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
        defer {
            m_parser.ast.deinit();
        };

        LOG_INF(
            "\n==================================================\nTest: %s",
            formatTokens(tokens).data())
        tokens.emplace_back(t_eof, "");
        Array<Token> token_ar{};
        defer {
            token_ar.deinit();
        };
        for (auto const &token : tokens) {
            token_ar.push() = createToken(token.id, token.text);
        }
        bool parse_ok = m_parser.parse(token_ar);
        EXPECT_TRUE(parse_ok);
        if (!parse_ok) {
            LOG_ERR("Error message: %.*s", m_parser.err.size, m_parser.err.data)
        }

        bool trees_are_equal = compare(m_parser.root, &expected);
        EXPECT_TRUE(trees_are_equal);
        if (!trees_are_equal) {
            auto expected_str = ast_inspect(&expected);
            auto actual_str = ast_inspect(m_parser.root);
            LOG_ERR("Expected tree: %.*s", expected_str.size, expected_str.data)
            LOG_ERR("Actual tree: %.*s", actual_str.size, actual_str.data)
        }
    }

private:
    std::string formatTokens(std::vector<TokenDescr> const &tokens) {
        std::ostringstream ss;
        for (auto const &token : tokens) {
            ss << token.text << " ";
        }
        return ss.str();
    }

    template <class ArrayType, class Pred>
    static bool compareArrays(ArrayType const &lhs, ArrayType const &rhs, const Pred &pred) {
        if (lhs.size != rhs.size) {
            return false;
        }

        for (size_t i = 0; i < lhs.size; i++) {
            if (!pred(lhs[i], rhs[i])) {
                return false;
            }
        }

        return true;
    }

    bool compareNodeArrays(NodeArray const &lhs, NodeArray const &rhs) {
        return compareArrays(lhs, rhs, [this](Node const &lhs, Node const &rhs) {
            return compare(lhs, rhs);
        });
    };

    bool compareNamedNodeArrays(NodeArray const &lhs, NodeArray const &rhs) {
        return compareArrays(lhs, rhs, [this](Node const &lhs, Node const &rhs) {
            return std_view(lhs.as.type_decl.name->text) == std_view(rhs.as.type_decl.name->text) &&
                   compare(lhs.as.named_node.node, rhs.as.named_node.node);
        });
    };

    bool compareNodePairArrays(NodeArray const &lhs, NodeArray const &rhs) {
        return compareArrays(lhs, rhs, [this](Node const &lhs, Node const &rhs) {
            return compare(lhs.as.binary.lhs, rhs.as.binary.lhs) &&
                   compare(lhs.as.binary.rhs, rhs.as.binary.rhs);
        });
    };

    bool compare(node_ref_t lhs_ref, node_ref_t rhs_ref) {
        if (!lhs_ref && !rhs_ref) {
            return true;
        }

        if (!lhs_ref || !rhs_ref) {
            return false;
        }

        Node const &lhs = *lhs_ref;
        Node const &rhs = *rhs_ref;

        return compare(lhs, rhs);
    }

    bool compare(Node const &lhs, Node const &rhs) {
        if (lhs.id != rhs.id) {
            return false;
        }

        switch (lhs.id) {
        default:
            return true;

#define U(TYPE, ID) case Node_##ID:
#include "nkl/core/nodes.inl"
            return compare(lhs.as.unary.arg, rhs.as.unary.arg);

#define B(TYPE, ID) case Node_##ID:
#include "nkl/core/nodes.inl"
            return compare(lhs.as.binary.lhs, rhs.as.binary.lhs) &&
                   compare(lhs.as.binary.rhs, rhs.as.binary.rhs);

        case Node_if:
        case Node_ternary:
            return compare(lhs.as.ternary.arg1, rhs.as.ternary.arg1) &&
                   compare(lhs.as.ternary.arg2, rhs.as.ternary.arg2) &&
                   compare(lhs.as.ternary.arg3, rhs.as.ternary.arg3);

        case Node_array:
        case Node_block:
        case Node_tuple:
        case Node_id_tuple:
        case Node_tuple_type:
            return compareNodeArrays(lhs.as.array.nodes, rhs.as.array.nodes);

        case Node_struct:
            return std_view(lhs.as.type_decl.name->text) == std_view(rhs.as.type_decl.name->text) &&
                   compareNamedNodeArrays(lhs.as.type_decl.fields, rhs.as.type_decl.fields);

        case Node_member:
            return compare(lhs.as.member.lhs, rhs.as.member.lhs) &&
                   std_view(lhs.as.member.name->text) == std_view(rhs.as.member.name->text);

        case Node_id:
        case Node_numeric_float:
        case Node_numeric_int:
        case Node_string_literal:
            return std_view(lhs.as.token.val->text) == std_view(rhs.as.token.val->text);

        case Node_call:
            return compare(lhs.as.call.lhs, rhs.as.call.lhs) &&
                   compareNodeArrays(lhs.as.call.args, rhs.as.call.args);

        case Node_var_decl:
            return std_view(lhs.as.var_decl.name->text) == std_view(rhs.as.var_decl.name->text) &&
                   compare(lhs.as.var_decl.type, rhs.as.var_decl.type) &&
                   compare(lhs.as.var_decl.value, rhs.as.var_decl.value);

        case Node_fn:
            return std_view(lhs.as.fn.sig.name->text) == std_view(rhs.as.fn.sig.name->text) &&
                   compareNamedNodeArrays(lhs.as.fn.sig.params, rhs.as.fn.sig.params) &&
                   compare(lhs.as.fn.sig.ret_type, rhs.as.fn.sig.ret_type) &&
                   compare(lhs.as.fn.body, rhs.as.fn.body);

        case Node_struct_literal:
            return compare(lhs.as.struct_literal.type, rhs.as.struct_literal.type) &&
                   compareNamedNodeArrays(
                       lhs.as.struct_literal.fields, rhs.as.struct_literal.fields);
        }
    }

protected:
    Parser m_parser{};
    Ast m_ast{};
};

} // namespace

TEST_F(parser, empty) {
    // clang-format off
    test_ok({},
            m_ast.make_nop());
    // clang-format on
}

TEST_F(parser, nullary) {
    // clang-format off
    test_ok({t_break, t_semi},
            m_ast.make_break());
    test_ok({t_continue, t_semi},
            m_ast.make_continue());
    test_ok({t_i16, t_semi},
            m_ast.make_i16());
    test_ok({t_i32, t_semi},
            m_ast.make_i32());
    test_ok({t_i64, t_semi},
            m_ast.make_i64());
    test_ok({t_i8, t_semi},
            m_ast.make_i8());
    test_ok({t_u16, t_semi},
            m_ast.make_u16());
    test_ok({t_u32, t_semi},
            m_ast.make_u32());
    test_ok({t_u64, t_semi},
            m_ast.make_u64());
    test_ok({t_u8, t_semi},
            m_ast.make_u8());
    test_ok({t_void, t_semi},
            m_ast.make_void());
    test_ok({t_f32, t_semi},
            m_ast.make_f32());
    test_ok({t_f64, t_semi},
            m_ast.make_f64());
    // clang-format on
}

TEST_F(parser, unary) {
    auto a = m_ast.make_id(mkt(t_id, "a"));

    TokenDescr t_a{t_id, "a"};

    // clang-format off
    test_ok({t_amper, t_a, t_semi},
            m_ast.make_addr(a));
    test_ok({t_tilde, t_a, t_semi},
            m_ast.make_compl(a));
    test_ok({t_aster, t_a, t_semi},
            m_ast.make_deref(a));
    test_ok({t_exclam, t_a, t_semi},
            m_ast.make_not(a));
    test_ok({t_return, t_a, t_semi},
            m_ast.make_return(a));
    test_ok({t_minus, t_a, t_semi},
            m_ast.make_uminus(a));
    test_ok({t_plus, t_a, t_semi},
            m_ast.make_uplus(a));
    // clang-format on
}

TEST_F(parser, binary) {
    auto a = m_ast.make_id(mkt(t_id, "a"));
    auto num = m_ast.make_numeric_int(mkt(t_int_const, "42"));

    TokenDescr t_a{t_id, "a"};
    TokenDescr t_num{t_int_const, "42"};

    // clang-format off
    test_ok({t_num, t_plus, t_num, t_semi},
            m_ast.make_add(num, num));
    test_ok({t_a, t_plus_eq, t_num, t_semi},
            m_ast.make_add_assign(a, num));
    test_ok({t_num, t_amper_2x, t_num, t_semi},
            m_ast.make_and(num, num));
    test_ok({t_a, t_amper_eq, t_num, t_semi},
            m_ast.make_and_assign(a, num));
    test_ok({t_a, t_eq, t_num, t_semi},
            m_ast.make_assign(a, num));
    test_ok({t_num, t_amper, t_num, t_semi},
            m_ast.make_bitand(num, num));
    test_ok({t_num, t_amper_2x_eq, t_num, t_semi},
            m_ast.make_bitand_assign(num, num));
    test_ok({t_num, t_bar, t_num, t_semi},
            m_ast.make_bitor(num, num));
    test_ok({t_num, t_bar_2x_eq, t_num, t_semi},
            m_ast.make_bitor_assign(num, num));
    test_ok({t_cast, t_par_l, t_f64, t_par_r, t_num, t_semi},
            m_ast.make_cast(m_ast.make_f64(), num));
    test_ok({t_a, t_colon_eq, t_num, t_semi},
            m_ast.make_colon_assign(a, num));
    test_ok({t_num, t_slash, t_num, t_semi},
            m_ast.make_div(num, num));
    test_ok({t_a, t_slash_eq, t_num, t_semi},
            m_ast.make_div_assign(a, num));
    test_ok({t_num, t_eq_2x, t_num, t_semi},
            m_ast.make_eq(num, num));
    test_ok({t_num, t_greater_eq, t_num, t_semi},
            m_ast.make_ge(num, num));
    test_ok({t_num, t_greater, t_num, t_semi},
            m_ast.make_gt(num, num));
    test_ok({t_a, t_bracket_l, t_num, t_bracket_r, t_semi},
            m_ast.make_index(a, num));
    test_ok({t_num, t_less_eq, t_num, t_semi},
            m_ast.make_le(num, num));
    test_ok({t_num, t_less_2x, t_num, t_semi},
            m_ast.make_lsh(num, num));
    test_ok({t_num, t_less_2x_eq, t_num, t_semi},
            m_ast.make_lsh_assign(num, num));
    test_ok({t_num, t_less, t_num, t_semi},
            m_ast.make_lt(num, num));
    test_ok({t_num, t_percent, t_num, t_semi},
            m_ast.make_mod(num, num));
    test_ok({t_a, t_percent_eq, t_num, t_semi},
            m_ast.make_mod_assign(a, num));
    test_ok({t_num, t_aster, t_num, t_semi},
            m_ast.make_mul(num, num));
    test_ok({t_a, t_aster_eq, t_num, t_semi},
            m_ast.make_mul_assign(a, num));
    test_ok({t_num, t_exclam_eq, t_num, t_semi},
            m_ast.make_ne(num, num));
    test_ok({t_num, t_bar_2x, t_num, t_semi},
            m_ast.make_or(num, num));
    test_ok({t_a, t_bar_eq, t_num, t_semi},
            m_ast.make_or_assign(a, num));
    test_ok({t_num, t_greater_2x, t_num, t_semi},
            m_ast.make_rsh(num, num));
    test_ok({t_a, t_greater_2x_eq, t_num, t_semi},
            m_ast.make_rsh_assign(a, num));
    test_ok({t_num, t_minus, t_num, t_semi},
            m_ast.make_sub(num, num));
    test_ok({t_a, t_minus_eq, t_num, t_semi},
            m_ast.make_sub_assign(a, num));
    test_ok({t_array_t, t_brace_l, t_f64, t_comma, t_num, t_brace_r, t_semi},
            m_ast.make_array_type(m_ast.make_f64(), num));
    test_ok({t_while, t_num, t_brace_l, t_a, t_semi, t_brace_r},
            m_ast.make_while(num, m_ast.make_scope(a)));
    test_ok({t_num, t_caret, t_num, t_semi},
            m_ast.make_xor(num, num));
    test_ok({t_a, t_caret_eq, t_num, t_semi},
            m_ast.make_xor_assign(a, num));
    // clang-format on
}

TEST_F(parser, ternary) {
    auto a = m_ast.make_id(mkt(t_id, "a"));
    auto b = m_ast.make_id(mkt(t_id, "b"));
    auto num = m_ast.make_numeric_int(mkt(t_int_const, "42"));

    TokenDescr t_a{t_id, "a"};
    TokenDescr t_b{t_id, "b"};
    TokenDescr t_num{t_int_const, "42"};

    // clang-format off
    test_ok({t_num, t_question, t_a, t_colon, t_b, t_semi},
            m_ast.make_ternary(num, a, b));
    test_ok({t_if, t_num, t_brace_l, t_a, t_semi, t_brace_r},
            m_ast.make_if(num, m_ast.make_scope(a), {}));
    test_ok({t_if, t_num, t_brace_l, t_a, t_semi, t_brace_r, t_else, t_brace_l, t_b, t_semi, t_brace_r},
            m_ast.make_if(num, m_ast.make_scope(a), m_ast.make_scope(b)));
    // clang-format on
}

TEST_F(parser, other) {
    auto id_a = mkt(t_id, "a");
    auto a = m_ast.make_id(id_a);
    auto id_b = mkt(t_id, "b");
    auto b = m_ast.make_id(id_b);
    auto id_A = mkt(t_id, "A");
    auto id_plus = mkt(t_id, "plus");
    auto num = m_ast.make_numeric_int(mkt(t_int_const, "42"));
    auto i64 = m_ast.make_i64();
    auto f64 = m_ast.make_f64();

    TokenDescr t_a{t_id, "a"};
    TokenDescr t_b{t_id, "b"};
    TokenDescr t_A{t_id, "A"};
    TokenDescr t_id_plus{t_id, "plus"};
    TokenDescr t_num{t_int_const, "42"};

    static constexpr const char *c_test_str = "hello";

    Node ab_ar[] = {a, b};
    Node i64f64_ar[] = {i64, f64};

    // clang-format off
    test_ok({t_bracket_l, t_a, t_comma, t_b, t_bracket_r, t_semi},
            m_ast.make_array({ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])}));
    test_ok({t_brace_l, t_a, t_semi, t_b, t_semi, t_brace_r, t_semi},
            m_ast.make_scope(m_ast.make_block({ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])})));
    test_ok({t_a, t_comma, t_b, t_semi},
            m_ast.make_id_tuple({ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])}));
    test_ok({t_tuple_t, t_brace_l, t_i64, t_comma, t_f64, t_brace_r, t_semi},
            m_ast.make_tuple_type({i64f64_ar, sizeof(i64f64_ar) / sizeof(i64f64_ar[0])}));
    test_ok({t_ptr_t, t_brace_l, t_ptr_t, t_brace_l, t_i8, t_brace_r, t_brace_r, t_semi},
            m_ast.make_ptr_type(m_ast.make_ptr_type(m_ast.make_i8())));
    // clang-format on

    NamedNode name_ar[] = {{id_a, m_ast.push(f64)}, {id_b, m_ast.push(f64)}};

    // clang-format off
    test_ok({t_struct, t_A, t_brace_l, t_a, t_colon, t_f64, t_semi, t_b, t_colon, t_f64, t_semi, t_brace_r},
            m_ast.make_struct(id_A, {name_ar, sizeof(name_ar) / sizeof(name_ar[0])}));
    test_ok({t_a, t_period, t_b, t_semi},
            m_ast.make_member(a, id_b));
    test_ok({{t_float_const, "3.14"}, t_semi},
            m_ast.make_numeric_float(mkt(t_float_const, "3.14")));
    test_ok({t_num, t_semi},
            m_ast.make_numeric_int(mkt(t_int_const, "42")));
    test_ok({t_a, t_semi},
            m_ast.make_id(id_a));
    test_ok({t_a, t_par_l, t_a, t_comma, t_b, t_par_r, t_semi},
            m_ast.make_call(a, {ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])}));
    test_ok({t_a, t_colon, t_f64, t_eq, t_num, t_semi},
            m_ast.make_var_decl(id_a, f64, num));
    test_ok({t_fn, t_id_plus, t_par_l, t_a, t_colon, t_f64, t_comma, t_b, t_colon, t_f64, t_par_r, t_minus_greater, t_i64, t_brace_l, t_return, t_a, t_plus, t_b, t_semi, t_brace_r},
            m_ast.make_fn( id_plus, {name_ar, sizeof(name_ar) / sizeof(name_ar[0])}, i64, m_ast.make_scope(m_ast.make_return(m_ast.make_add(a, b)))));
    test_ok({{t_str_const, c_test_str}, t_semi},
            m_ast.make_string_literal(mkt(t_str_const, c_test_str)));
    // test_ok({t_new, t_A, t_brace_l, t_a, t_colon, t_f64, t_comma, t_b, t_colon, t_f64, t_brace_r, t_semi},
    //         m_ast.make_struct_literal(A, {{id_a, f64}, {id_b, f64}}));
    // clang-format on
}
