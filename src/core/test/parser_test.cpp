#include "nkl/core/parser.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>

#include <gtest/gtest.h>

#include "nk/common/logger.hpp"
#include "nk/vm/vm.hpp"

namespace {

using namespace nk;
using namespace nkl;

LOG_USE_SCOPE(nkl::parser::test)

Token mkt(ETokenID id = Token_eof, const char *text = "") {
    return Token{{text, std::strlen(text)}, 0, 0, 0, id};
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

    void test_ok(std::vector<TokenDescr> tokens, node_ref_t expected) {
        DEFER({ m_parser.ast.deinit(); })

        LOG_INF(
            "\n==================================================\nTest: %s",
            formatTokens(tokens).data())
        tokens.emplace_back(Token_eof, "");
        Array<Token> token_ar{};
        DEFER({ token_ar.deinit(); })
        for (auto const &token : tokens) {
            token_ar.push() = mkt(token.id, token.text);
        }
        bool parse_ok = m_parser.parse(token_ar);
        EXPECT_TRUE(parse_ok);
        if (!parse_ok) {
            reportError();
        }

        bool trees_are_equal = compare(m_parser.root, expected);
        EXPECT_TRUE(trees_are_equal);
        if (!trees_are_equal) {
            auto expected_str = ast_inspect(expected);
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
            return lhs.as.named_node.name == rhs.as.named_node.name &&
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
        case Node_tuple_type:
            return compareNodeArrays(lhs.as.array.nodes, rhs.as.array.nodes);

        case Node_struct:
            return lhs.as.type_decl.name == rhs.as.type_decl.name &&
                   compareNamedNodeArrays(lhs.as.type_decl.fields, rhs.as.type_decl.fields);

        case Node_member:
            return compare(lhs.as.member.lhs, rhs.as.member.lhs) &&
                   lhs.as.member.name == rhs.as.member.name;

        case Node_numeric_f64:
            return lhs.as.numeric.val.f64 == rhs.as.numeric.val.f64;

        case Node_numeric_i64:
            return lhs.as.numeric.val.i64 == rhs.as.numeric.val.i64;

        case Node_id:
            return lhs.as.id.name == rhs.as.id.name;

        case Node_call:
            return compare(lhs.as.call.lhs, rhs.as.call.lhs) &&
                   compareNodeArrays(lhs.as.call.args, rhs.as.call.args);

        case Node_var_decl:
            return lhs.as.var_decl.name == rhs.as.var_decl.name &&
                   compare(lhs.as.var_decl.type, rhs.as.var_decl.type) &&
                   compare(lhs.as.var_decl.value, rhs.as.var_decl.value);

        case Node_fn:
            return lhs.as.fn.sig.name == rhs.as.fn.sig.name &&
                   compareNamedNodeArrays(lhs.as.fn.sig.params, rhs.as.fn.sig.params) &&
                   compare(lhs.as.fn.sig.ret_type, rhs.as.fn.sig.ret_type) &&
                   compare(lhs.as.fn.body, rhs.as.fn.body);

        case Node_string_literal:
            return std_view(lhs.as.str.val) == std_view(rhs.as.str.val);

        case Node_struct_literal:
            return compare(lhs.as.struct_literal.type, rhs.as.struct_literal.type) &&
                   compareNamedNodeArrays(
                       lhs.as.struct_literal.fields, rhs.as.struct_literal.fields);
        }
    }

    void reportError() {
        LOG_ERR("Error message: %.*s", m_parser.err.size, m_parser.err.data)
    }

protected:
    Parser m_parser{};
    Ast m_ast{};
};

} // namespace

TEST_F(parser, empty) {
    test_ok({}, m_ast.push(m_ast.make_nop()));
}

TEST_F(parser, nullary) {
    test_ok({Token_break, Token_semi}, m_ast.push(m_ast.make_break()));
    test_ok({Token_continue, Token_semi}, m_ast.push(m_ast.make_continue()));
    test_ok({Token_i16, Token_semi}, m_ast.push(m_ast.make_i16()));
    test_ok({Token_i32, Token_semi}, m_ast.push(m_ast.make_i32()));
    test_ok({Token_i64, Token_semi}, m_ast.push(m_ast.make_i64()));
    test_ok({Token_i8, Token_semi}, m_ast.push(m_ast.make_i8()));
    test_ok({Token_u16, Token_semi}, m_ast.push(m_ast.make_u16()));
    test_ok({Token_u32, Token_semi}, m_ast.push(m_ast.make_u32()));
    test_ok({Token_u64, Token_semi}, m_ast.push(m_ast.make_u64()));
    test_ok({Token_u8, Token_semi}, m_ast.push(m_ast.make_u8()));
    test_ok({Token_void, Token_semi}, m_ast.push(m_ast.make_void()));
    test_ok({Token_f32, Token_semi}, m_ast.push(m_ast.make_f32()));
    test_ok({Token_f64, Token_semi}, m_ast.push(m_ast.make_f64()));
}

TEST_F(parser, unary) {
    auto a = m_ast.push(m_ast.make_id(cs2id("a")));

    TokenDescr Token_a{Token_id, "a"};

    test_ok({Token_amper, Token_a, Token_semi}, m_ast.push(m_ast.make_addr(a)));
    test_ok({Token_tilde, Token_a, Token_semi}, m_ast.push(m_ast.make_compl(a)));
    test_ok({Token_aster, Token_a, Token_semi}, m_ast.push(m_ast.make_deref(a)));
    test_ok({Token_exclam, Token_a, Token_semi}, m_ast.push(m_ast.make_not(a)));
    test_ok({Token_return, Token_a, Token_semi}, m_ast.push(m_ast.make_return(a)));
    test_ok({Token_minus, Token_a, Token_semi}, m_ast.push(m_ast.make_uminus(a)));
    test_ok({Token_plus, Token_a, Token_semi}, m_ast.push(m_ast.make_uplus(a)));
}

TEST_F(parser, binary) {
    auto a = m_ast.push(m_ast.make_id(cs2id("a")));
    auto num = m_ast.push(m_ast.make_numeric_i64(42));

    TokenDescr Token_a{Token_id, "a"};
    TokenDescr Token_num{Token_int_const, "42"};

    test_ok({Token_num, Token_plus, Token_num, Token_semi}, m_ast.push(m_ast.make_add(num, num)));
    test_ok(
        {Token_a, Token_plus_eq, Token_num, Token_semi}, m_ast.push(m_ast.make_add_assign(a, num)));
    test_ok(
        {Token_num, Token_amper_dbl, Token_num, Token_semi}, m_ast.push(m_ast.make_and(num, num)));
    test_ok(
        {Token_a, Token_amper_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_and_assign(a, num)));
    test_ok({Token_a, Token_eq, Token_num, Token_semi}, m_ast.push(m_ast.make_assign(a, num)));
    test_ok(
        {Token_num, Token_amper, Token_num, Token_semi}, m_ast.push(m_ast.make_bitand(num, num)));
    test_ok(
        {Token_num, Token_amper_dbl_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_bitand_assign(num, num)));
    test_ok({Token_num, Token_bar, Token_num, Token_semi}, m_ast.push(m_ast.make_bitor(num, num)));
    test_ok(
        {Token_num, Token_bar_dbl_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_bitor_assign(num, num)));
    test_ok(
        {Token_cast, Token_par_l, Token_f64, Token_par_r, Token_num, Token_semi},
        m_ast.push(m_ast.make_cast(m_ast.push(m_ast.make_f64()), num)));
    test_ok(
        {Token_a, Token_colon_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_colon_assign(a, num)));
    test_ok({Token_num, Token_slash, Token_num, Token_semi}, m_ast.push(m_ast.make_div(num, num)));
    test_ok(
        {Token_a, Token_slash_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_div_assign(a, num)));
    test_ok({Token_num, Token_eq_dbl, Token_num, Token_semi}, m_ast.push(m_ast.make_eq(num, num)));
    test_ok(
        {Token_num, Token_greater_eq, Token_num, Token_semi}, m_ast.push(m_ast.make_ge(num, num)));
    test_ok({Token_num, Token_greater, Token_num, Token_semi}, m_ast.push(m_ast.make_gt(num, num)));
    test_ok(
        {Token_a, Token_bracket_l, Token_num, Token_bracket_r, Token_semi},
        m_ast.push(m_ast.make_index(a, num)));
    test_ok({Token_num, Token_less_eq, Token_num, Token_semi}, m_ast.push(m_ast.make_le(num, num)));
    test_ok(
        {Token_num, Token_less_dbl, Token_num, Token_semi}, m_ast.push(m_ast.make_lsh(num, num)));
    test_ok(
        {Token_num, Token_less_dbl_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_lsh_assign(num, num)));
    test_ok({Token_num, Token_less, Token_num, Token_semi}, m_ast.push(m_ast.make_lt(num, num)));
    test_ok(
        {Token_num, Token_percent, Token_num, Token_semi}, m_ast.push(m_ast.make_mod(num, num)));
    test_ok(
        {Token_a, Token_percent_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_mod_assign(a, num)));
    test_ok({Token_num, Token_aster, Token_num, Token_semi}, m_ast.push(m_ast.make_mul(num, num)));
    test_ok(
        {Token_a, Token_aster_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_mul_assign(a, num)));
    test_ok(
        {Token_num, Token_exclam_eq, Token_num, Token_semi}, m_ast.push(m_ast.make_ne(num, num)));
    test_ok({Token_num, Token_bar_dbl, Token_num, Token_semi}, m_ast.push(m_ast.make_or(num, num)));
    test_ok(
        {Token_a, Token_bar_eq, Token_num, Token_semi}, m_ast.push(m_ast.make_or_assign(a, num)));
    test_ok(
        {Token_num, Token_greater_dbl, Token_num, Token_semi},
        m_ast.push(m_ast.make_rsh(num, num)));
    test_ok(
        {Token_a, Token_greater_dbl_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_rsh_assign(a, num)));
    test_ok({Token_num, Token_minus, Token_num, Token_semi}, m_ast.push(m_ast.make_sub(num, num)));
    test_ok(
        {Token_a, Token_minus_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_sub_assign(a, num)));
    test_ok(
        {Token_array_t, Token_par_l, Token_f64, Token_comma, Token_num, Token_par_r, Token_semi},
        m_ast.push(m_ast.make_array_type(m_ast.push(m_ast.make_f64()), num)));
    test_ok(
        {Token_while, Token_num, Token_brace_l, Token_a, Token_semi, Token_brace_r},
        m_ast.push(m_ast.make_while(num, a)));
    test_ok({Token_num, Token_caret, Token_num, Token_semi}, m_ast.push(m_ast.make_xor(num, num)));
    test_ok(
        {Token_a, Token_caret_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_xor_assign(a, num)));
}

TEST_F(parser, ternary) {
    auto a = m_ast.push(m_ast.make_id(cs2id("a")));
    auto b = m_ast.push(m_ast.make_id(cs2id("b")));
    auto num = m_ast.push(m_ast.make_numeric_i64(42));

    TokenDescr Token_a{Token_id, "a"};
    TokenDescr Token_b{Token_id, "b"};
    TokenDescr Token_num{Token_int_const, "42"};

    test_ok(
        {Token_num, Token_question, Token_a, Token_colon, Token_b, Token_semi},
        m_ast.push(m_ast.make_ternary(num, a, b)));

    test_ok(
        {Token_if, Token_num, Token_brace_l, Token_a, Token_semi, Token_brace_r},
        m_ast.push(m_ast.make_if(num, a, n_none_ref)));

    test_ok(
        {Token_if,
         Token_num,
         Token_brace_l,
         Token_a,
         Token_semi,
         Token_brace_r,
         Token_else,
         Token_brace_l,
         Token_b,
         Token_semi,
         Token_brace_r},
        m_ast.push(m_ast.make_if(num, a, b)));
}

TEST_F(parser, other) {
    auto id_a = cs2id("a");
    auto a = m_ast.make_id(id_a);
    auto id_b = cs2id("b");
    auto b = m_ast.make_id(id_b);
    auto id_A = cs2id("A");
    auto id_plus = cs2id("plus");
    auto num = m_ast.push(m_ast.make_numeric_i64(42));
    auto i64 = m_ast.make_i64();
    auto f64 = m_ast.make_f64();

    TokenDescr Token_a{Token_id, "a"};
    TokenDescr Token_b{Token_id, "b"};
    TokenDescr Token_A{Token_id, "A"};
    TokenDescr Token_id_plus{Token_id, "plus"};
    TokenDescr Token_num{Token_int_const, "42"};

    static constexpr const char *c_test_str = "hello";

    Node ab_ar[] = {a, b};
    Node i64f64_ar[] = {i64, f64};

    test_ok(
        {Token_bracket_l, Token_a, Token_comma, Token_b, Token_bracket_r, Token_semi},
        m_ast.push(m_ast.make_array({ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])})));

    test_ok(
        {Token_brace_l, Token_a, Token_semi, Token_b, Token_semi, Token_brace_r, Token_semi},
        m_ast.push(m_ast.make_block({ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])})));

    test_ok(
        {Token_a, Token_comma, Token_b, Token_semi},
        m_ast.push(m_ast.make_tuple({ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])})));

    test_ok(
        {Token_tuple_t, Token_par_l, Token_i64, Token_comma, Token_f64, Token_par_r, Token_semi},
        m_ast.push(m_ast.make_tuple_type({i64f64_ar, sizeof(i64f64_ar) / sizeof(i64f64_ar[0])})));

    NamedNode name_ar[] = {{id_a, m_ast.push(f64)}, {id_b, m_ast.push(f64)}};

    test_ok(
        {Token_struct,
         Token_A,
         Token_brace_l,
         Token_a,
         Token_colon,
         Token_f64,
         Token_semi,
         Token_b,
         Token_colon,
         Token_f64,
         Token_semi,
         Token_brace_r},
        m_ast.push(m_ast.make_struct(id_A, {name_ar, sizeof(name_ar) / sizeof(name_ar[0])})));

    test_ok(
        {Token_a, Token_period, Token_b, Token_semi},
        m_ast.push(m_ast.make_member(m_ast.push(a), id_b)));

    test_ok({{Token_float_const, "3.14"}, Token_semi}, m_ast.push(m_ast.make_numeric_f64(3.14)));

    test_ok({Token_num, Token_semi}, m_ast.push(m_ast.make_numeric_i64(42)));

    test_ok({Token_a, Token_semi}, m_ast.push(m_ast.make_id(id_a)));

    test_ok(
        {Token_a, Token_par_l, Token_a, Token_comma, Token_b, Token_par_r, Token_semi},
        m_ast.push(m_ast.make_call(m_ast.push(a), {ab_ar, sizeof(ab_ar) / sizeof(ab_ar[0])})));

    test_ok(
        {Token_a, Token_colon, Token_f64, Token_eq, Token_num, Token_semi},
        m_ast.push(m_ast.make_var_decl(id_a, m_ast.push(f64), num)));

    test_ok(
        {Token_fn,    Token_id_plus,       Token_par_l, Token_a,       Token_colon,
         Token_f64,   Token_comma,         Token_b,     Token_colon,   Token_f64,
         Token_par_r, Token_minus_greater, Token_i64,   Token_brace_l, Token_return,
         Token_a,     Token_plus,          Token_b,     Token_semi,    Token_brace_r},
        m_ast.push(m_ast.make_fn(
            id_plus,
            {name_ar, sizeof(name_ar) / sizeof(name_ar[0])},
            m_ast.push(i64),
            m_ast.push(
                m_ast.make_return(m_ast.push(m_ast.make_add(m_ast.push(a), m_ast.push(b))))))));

    test_ok(
        {{Token_str_const, c_test_str}, Token_semi},
        m_ast.push(m_ast.make_string_literal(cs2s(c_test_str))));

    // test_ok(
    //     {Token_new,
    //      Token_A,
    //      Token_brace_l,
    //      Token_a,
    //      Token_colon,
    //      Token_f64,
    //      Token_comma,
    //      Token_b,
    //      Token_colon,
    //      Token_f64,
    //      Token_brace_r,
    //      Token_semi},
    //     m_ast.push(m_ast.make_struct_literal(
    //         m_ast.push(A), {{id_a, m_ast.push(f64)}, {id_b, m_ast.push(f64)}})));
}
