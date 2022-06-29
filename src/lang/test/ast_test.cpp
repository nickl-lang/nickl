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

    LangAst m_ast{};
    StackAllocator m_allocator{};

    Node m_node{};
};

/// @TODO Expect something in lang/ast_test

TEST_F(ast, nullary) {
#define N(TYPE, ID) test(m_ast.make_##ID());
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, unary) {
#define U(TYPE, ID) test(m_ast.make_##ID(n_t_hello));
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, binary) {
#define B(TYPE, ID) test(m_ast.make_##ID(n_t_hello, n_t_hello));
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, ternary) {
    test(m_ast.make_if(n_t_hello, n_t_hello, n_t_hello));
    test(m_ast.make_ternary(n_t_hello, n_t_hello, n_t_hello));
}

TEST_F(ast, array) {
    Node const nodes[] = {n_t_hello, n_t_hello, n_t_hello};
    NodeArray const ar{nodes, sizeof(nodes) / sizeof(nodes[0])};

    test(m_ast.make_array(ar));
    test(m_ast.make_block(ar));
    test(m_ast.make_tuple(ar));
    test(m_ast.make_id_tuple(ar));
    test(m_ast.make_tuple_type(ar));
}

TEST_F(ast, token) {
    test(m_ast.make_id(&t_hello));
    test(m_ast.make_id(&t_hello));
    test(m_ast.make_numeric_float(&t_hello));
    test(m_ast.make_numeric_int(&t_hello));
    test(m_ast.make_string_literal(&t_hello));
    test(m_ast.make_escaped_string_literal(&t_hello));
}

TEST_F(ast, other) {
    Node const nodes[] = {n_t_hello, n_t_hello, n_t_hello};
    NodeArray const ar{nodes, sizeof(nodes) / sizeof(nodes[0])};

    NamedNode nnodes[] = {{&t_hello, &n_t_hello}, {&t_hello, &n_t_hello}, {&t_hello, &n_t_hello}};
    NamedNodeArray nn_ar{nnodes, sizeof(nnodes) / sizeof(nnodes[0])};

    test(m_ast.make_member(n_t_hello, &t_hello));
    test(m_ast.make_struct(&t_hello, nn_ar));
    test(m_ast.make_call(n_t_hello, ar));
    test(m_ast.make_fn(&t_hello, nn_ar, n_t_hello, n_t_hello));
    test(m_ast.make_struct_literal(n_t_hello, nn_ar));
    test(m_ast.make_var_decl(&t_hello, n_t_hello, n_t_hello));
}
