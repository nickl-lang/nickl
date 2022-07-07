#include "nkl/lang/ast.hpp"

#include <gtest/gtest.h>

#include "nk/mem/stack_allocator.hpp"
#include "nk/str/dynamic_string_builder.hpp"
#include "nk/utils/logger.h"
#include "nkl/core/test/ast_utils.hpp"

using namespace nkl;
using namespace nkl::test;

LOG_USE_SCOPE(test::ast);

struct ast : testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_ast.init();

        id_init();

        m_sb.reserve(1000);

        t_hello = Token{cs2s("hello"), 0, 0, 0};
        n_t_hello = Node{{{&t_hello, {}}, {}, {}}, nodeId(Node_id)};

        t_x = Token{cs2s("x"), 0, 0, 0};
        t_y = Token{cs2s("y"), 0, 0, 0};
        t_z = Token{cs2s("z"), 0, 0, 0};
        t_w = Token{cs2s("w"), 0, 0, 0};

        n_t_x = Node{{{&t_x, {}}, {}, {}}, nodeId(Node_id)};
        n_t_y = Node{{{&t_y, {}}, {}, {}}, nodeId(Node_id)};
        n_t_z = Node{{{&t_z, {}}, {}, {}}, nodeId(Node_id)};
        n_t_w = Node{{{&t_w, {}}, {}, {}}, nodeId(Node_id)};
    }

    void TearDown() override {
        id_deinit();

        m_ast.deinit();
        m_allocator.deinit();

        m_sb.deinit();
    }

    void test(Node const &node) {
        m_node = node;

        auto str = ast_inspect(&m_node, m_sb).moveStr(m_allocator);
        LOG_INF("Test: %.*s", str.size, str.data);
    }

    bool expect_ast(NodeRef actual, NodeRef expected) {
        bool is_ast_equal = ast_equal(actual, expected);
        if (!is_ast_equal) {
            auto actual_str = ast_inspect(actual, m_sb).moveStr(m_allocator);
            auto expected_str = ast_inspect(expected, m_sb).moveStr(m_allocator);
            LOG_ERR(
                "\nActual: %.*s\nExpected: %.*s",
                actual_str.size,
                actual_str.data,
                expected_str.size,
                expected_str.data);
        }
        return is_ast_equal;
    }

    Token t_hello{};
    Node n_t_hello{};

    Token t_x{};
    Token t_y{};
    Token t_z{};
    Token t_w{};

    Node n_t_x{};
    Node n_t_y{};
    Node n_t_z{};
    Node n_t_w{};

    LangAst m_ast{};
    StackAllocator m_allocator{};
    DynamicStringBuilder m_sb{};

    Node m_node{};
};

#define EXPECT_AST(ACTUAL, EXPECTED)                                   \
    {                                                                  \
        bool const is_ast_equal = expect_ast((ACTUAL), (EXPECTED));    \
        EXPECT_TRUE(is_ast_equal) << "Actual expr: " << #ACTUAL        \
                                  << "\nExpected expr: " << #EXPECTED; \
    }

#define EXPECT_AST_AR(ACTUAL, EXPECTED)                 \
    {                                                   \
        auto const &actual_ar = (ACTUAL);               \
        auto const &expected_ar = (EXPECTED);           \
        EXPECT_EQ(actual_ar.size, expected_ar.size);    \
        for (size_t i = 0; i < actual_ar.size; i++) {   \
            EXPECT_AST(&actual_ar[i], &expected_ar[i]); \
        }                                               \
    }

#define EXPECT_AST_NN_AR(ACTUAL, EXPECTED)                      \
    {                                                           \
        auto const &actual_ar = (ACTUAL);                       \
        auto const &expected_ar = (EXPECTED);                   \
        EXPECT_EQ(actual_ar.size, expected_ar.size);            \
        for (size_t i = 0; i < actual_ar.size; i++) {           \
            EXPECT_EQ(actual_ar[i].name, expected_ar[i].name);  \
            EXPECT_AST(actual_ar[i].node, expected_ar[i].node); \
        }                                                       \
    }

TEST_F(ast, nullary) {
#define N(ID) test(m_ast.CAT(make_, ID)());
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, unary) {
#define U(ID)                                     \
    test(m_ast.CAT(make_, ID)(n_t_x));            \
    EXPECT_EQ(nodeId(CAT(Node_, ID)), m_node.id); \
    EXPECT_AST(Node_unary_arg(&m_node), &n_t_x);
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, binary) {
#define B(ID)                                     \
    test(m_ast.CAT(make_, ID)(n_t_x, n_t_y));     \
    EXPECT_EQ(nodeId(CAT(Node_, ID)), m_node.id); \
    EXPECT_AST(Node_binary_lhs(&m_node), &n_t_x); \
    EXPECT_AST(Node_binary_rhs(&m_node), &n_t_y);
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, ternary) {
    test(m_ast.make_if(n_t_x, n_t_y, n_t_z));
    EXPECT_EQ(nodeId(Node_if), m_node.id);
    EXPECT_AST(Node_ternary_cond(&m_node), &n_t_x);
    EXPECT_AST(Node_ternary_then_clause(&m_node), &n_t_y);
    EXPECT_AST(Node_ternary_else_clause(&m_node), &n_t_z);

    test(m_ast.make_ternary(n_t_x, n_t_y, n_t_z));
    EXPECT_EQ(nodeId(Node_ternary), m_node.id);
    EXPECT_AST(Node_ternary_cond(&m_node), &n_t_x);
    EXPECT_AST(Node_ternary_then_clause(&m_node), &n_t_y);
    EXPECT_AST(Node_ternary_else_clause(&m_node), &n_t_z);
}

TEST_F(ast, array) {
    ARRAY_SLICE_INIT(Node const, nodes, n_t_x, n_t_y, n_t_z);

    test(m_ast.make_array(nodes));
    EXPECT_EQ(nodeId(Node_array), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes);

    test(m_ast.make_block(nodes));
    EXPECT_EQ(nodeId(Node_block), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes);

    test(m_ast.make_tuple(nodes));
    EXPECT_EQ(nodeId(Node_tuple), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes);

    test(m_ast.make_id_tuple(nodes));
    EXPECT_EQ(nodeId(Node_id_tuple), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes);

    test(m_ast.make_tuple_type(nodes));
    EXPECT_EQ(nodeId(Node_tuple_type), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes);
}

TEST_F(ast, token) {
    test(m_ast.make_id(&t_hello));
    EXPECT_EQ(nodeId(Node_id), m_node.id);
    EXPECT_EQ(Node_token_name(&m_node), &t_hello);

    test(m_ast.make_numeric_float(&t_hello));
    EXPECT_EQ(nodeId(Node_numeric_float), m_node.id);
    EXPECT_EQ(Node_token_name(&m_node), &t_hello);

    test(m_ast.make_numeric_int(&t_hello));
    EXPECT_EQ(nodeId(Node_numeric_int), m_node.id);
    EXPECT_EQ(Node_token_name(&m_node), &t_hello);

    test(m_ast.make_string_literal(&t_hello));
    EXPECT_EQ(nodeId(Node_string_literal), m_node.id);
    EXPECT_EQ(Node_token_name(&m_node), &t_hello);

    test(m_ast.make_escaped_string_literal(&t_hello));
    EXPECT_EQ(nodeId(Node_escaped_string_literal), m_node.id);
    EXPECT_EQ(Node_token_name(&m_node), &t_hello);
}

TEST_F(ast, other) {
    ARRAY_SLICE_INIT(Node const, nodes, n_t_x, n_t_y, n_t_z);

    NamedNode nn_x{&t_x, &n_t_hello};
    NamedNode nn_y{&t_y, &n_t_hello};
    NamedNode nn_z{&t_z, &n_t_hello};
    NamedNode nn_w{&t_w, &n_t_hello};

    ARRAY_SLICE_INIT(NamedNode const, nnodes, nn_x, nn_y, nn_z, nn_w);

    test(m_ast.make_member(n_t_hello, &t_hello));
    EXPECT_EQ(nodeId(Node_member), m_node.id);
    EXPECT_AST(Node_member_lhs(&m_node), &n_t_hello);
    EXPECT_EQ(Node_member_name(&m_node), &t_hello);

    test(m_ast.make_struct(&t_hello, nnodes));
    EXPECT_EQ(nodeId(Node_struct), m_node.id);
    EXPECT_EQ(Node_struct_name(&m_node), &t_hello);
    EXPECT_AST_NN_AR(Node_struct_fields(&m_node), nnodes);

    test(m_ast.make_call(n_t_hello, nodes));
    EXPECT_EQ(nodeId(Node_call), m_node.id);
    EXPECT_AST(Node_call_lhs(&m_node), &n_t_hello);
    EXPECT_AST_AR(Node_call_args(&m_node), nodes);

    test(m_ast.make_fn(&t_hello, nnodes, n_t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_fn), m_node.id);
    EXPECT_EQ(Node_fn_name(&m_node), &t_hello);
    EXPECT_AST_NN_AR(Node_fn_params(&m_node), nnodes);
    EXPECT_AST(Node_fn_ret_type(&m_node), &n_t_hello);
    EXPECT_AST(Node_fn_body(&m_node), &n_t_hello);

    test(m_ast.make_struct_literal(n_t_hello, nnodes));
    EXPECT_EQ(nodeId(Node_struct_literal), m_node.id);
    EXPECT_AST(Node_struct_literal_type(&m_node), &n_t_hello);
    EXPECT_AST_NN_AR(Node_struct_literal_fields(&m_node), nnodes);

    test(m_ast.make_var_decl(&t_hello, n_t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_var_decl), m_node.id);
    EXPECT_EQ(Node_var_decl_name(&m_node), &t_hello);
    EXPECT_AST(Node_var_decl_type(&m_node), &n_t_hello);
    EXPECT_AST(Node_var_decl_value(&m_node), &n_t_hello);
}
