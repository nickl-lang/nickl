#include "nkl/lang/ast.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/stack_allocator.hpp"
#include "nkl/core/test/ast_utils.hpp"

using namespace nkl;
using namespace nkl::test;

LOG_USE_SCOPE(test::ast);

struct ast : testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_ast.init();

        id_init();

        t_hello = Token{cs2s("hello"), 0, 0, 0};
        n_t_hello = Node{{{&t_hello, {}}, {}, {}}, 0};

        t_x = Token{cs2s("x"), 0, 0, 0};
        t_y = Token{cs2s("y"), 0, 0, 0};
        t_z = Token{cs2s("z"), 0, 0, 0};
        t_w = Token{cs2s("w"), 0, 0, 0};

        n_t_x = Node{{{&t_x, {}}, {}, {}}, 0};
        n_t_y = Node{{{&t_y, {}}, {}, {}}, 0};
        n_t_z = Node{{{&t_z, {}}, {}, {}}, 0};
        n_t_w = Node{{{&t_w, {}}, {}, {}}, 0};
    }

    void TearDown() override {
        id_deinit();

        m_ast.deinit();
        m_allocator.deinit();
    }

    void test(Node const &node) {
        m_node = node;

        auto str = ast_inspect(&m_node, m_allocator);
        LOG_INF("Test: %.*s", str.size, str.data);
    }

    bool expect_ast(NodeRef actual, NodeRef expected) {
        bool is_ast_equal = ast_equal(actual, expected);
        if (!is_ast_equal) {
            auto actual_str = ast_inspect(actual, m_allocator);
            auto expected_str = ast_inspect(expected, m_allocator);
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
#define N(TYPE, ID) test(m_ast.CAT(make_, ID)());
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, unary) {
#define U(TYPE, ID)                               \
    test(m_ast.CAT(make_, ID)(n_t_x));            \
    EXPECT_EQ(nodeId(CAT(Node_, ID)), m_node.id); \
    EXPECT_AST(Node_unary_arg(&m_node), &n_t_x);
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, binary) {
#define B(TYPE, ID)                               \
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
    Node const nodes[] = {n_t_x, n_t_y, n_t_y};
    NodeArray const nodes_ar{nodes, sizeof(nodes) / sizeof(nodes[0])};

    test(m_ast.make_array(nodes_ar));
    EXPECT_EQ(nodeId(Node_array), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes_ar);

    test(m_ast.make_block(nodes_ar));
    EXPECT_EQ(nodeId(Node_block), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes_ar);

    test(m_ast.make_tuple(nodes_ar));
    EXPECT_EQ(nodeId(Node_tuple), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes_ar);

    test(m_ast.make_id_tuple(nodes_ar));
    EXPECT_EQ(nodeId(Node_id_tuple), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes_ar);

    test(m_ast.make_tuple_type(nodes_ar));
    EXPECT_EQ(nodeId(Node_tuple_type), m_node.id);
    EXPECT_AST_AR(Node_array_nodes(&m_node), nodes_ar);
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
    Node const nodes[] = {n_t_x, n_t_y, n_t_y};
    NodeArray const nodes_ar{nodes, sizeof(nodes) / sizeof(nodes[0])};

    NamedNode nn_x{&t_x, &n_t_hello};
    NamedNode nn_y{&t_y, &n_t_hello};
    NamedNode nn_z{&t_z, &n_t_hello};
    NamedNode nn_w{&t_w, &n_t_hello};

    NamedNode nnodes[] = {nn_x, nn_y, nn_z, nn_w};
    NamedNodeArray nnodes_ar{nnodes, sizeof(nnodes) / sizeof(nnodes[0])};

    test(m_ast.make_member(n_t_hello, &t_hello));
    EXPECT_EQ(nodeId(Node_member), m_node.id);
    EXPECT_AST(Node_member_lhs(&m_node), &n_t_hello);
    EXPECT_EQ(Node_member_name(&m_node), &t_hello);

    test(m_ast.make_struct(&t_hello, nnodes_ar));
    EXPECT_EQ(nodeId(Node_struct), m_node.id);
    EXPECT_EQ(Node_struct_name(&m_node), &t_hello);
    EXPECT_AST_NN_AR(Node_struct_fields(&m_node), nnodes_ar);

    test(m_ast.make_call(n_t_hello, nodes_ar));
    EXPECT_EQ(nodeId(Node_call), m_node.id);
    EXPECT_AST(Node_call_lhs(&m_node), &n_t_hello);
    EXPECT_AST_AR(Node_call_args(&m_node), nodes_ar);

    test(m_ast.make_fn(&t_hello, nnodes_ar, n_t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_fn), m_node.id);
    EXPECT_EQ(Node_fn_name(&m_node), &t_hello);
    EXPECT_AST_NN_AR(Node_fn_params(&m_node), nnodes_ar);
    EXPECT_AST(Node_fn_ret_type(&m_node), &n_t_hello);
    EXPECT_AST(Node_fn_body(&m_node), &n_t_hello);

    test(m_ast.make_struct_literal(n_t_hello, nnodes_ar));
    EXPECT_EQ(nodeId(Node_struct_literal), m_node.id);
    EXPECT_AST(Node_struct_literal_type(&m_node), &n_t_hello);
    EXPECT_AST_NN_AR(Node_struct_literal_fields(&m_node), nnodes_ar);

    test(m_ast.make_var_decl(&t_hello, n_t_hello, n_t_hello));
    EXPECT_EQ(nodeId(Node_var_decl), m_node.id);
    EXPECT_EQ(Node_var_decl_name(&m_node), &t_hello);
    EXPECT_AST(Node_var_decl_type(&m_node), &n_t_hello);
    EXPECT_AST(Node_var_decl_value(&m_node), &n_t_hello);
}
