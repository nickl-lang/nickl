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
    }

    void TearDown() override {
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

    enum ETestNodeId {
        TestNode_t_hello = 42,
    };

    Token const t_hello{cs2s("hello"), 1, 2, 3};
    Node const n_t_hello{{{&t_hello, {}}, {}, {}}, TestNode_t_hello};

    LangAst m_ast{};
    StackAllocator m_allocator{};

    Node m_node{};
};

#define EXPECT_AST(EXPECTED, ACTUAL)                                 \
    {                                                                \
        bool const is_ast_equal = expect_ast((EXPECTED), (ACTUAL));  \
        EXPECT_TRUE(is_ast_equal) << "Actual expr: " << #EXPECTED    \
                                  << "\nExpected expr: " << #ACTUAL; \
    }

#define EXPECT_NODE0(ID) EXPECT_NODE1(ID, n_none)
#define EXPECT_NODE1(ID, ARG1) EXPECT_NODE2(ID, ARG1, n_none)
#define EXPECT_NODE2(ID, ARG1, ARG2) EXPECT_NODE3(ID, ARG1, ARG2, n_none)
#define EXPECT_NODE3(ID, ARG1, ARG2, ARG3)         \
    EXPECT_EQ(m_node.id, ID);                      \
    EXPECT_AST(m_node.arg[0].nodes.begin(), ARG1); \
    EXPECT_AST(m_node.arg[1].nodes.begin(), ARG2); \
    EXPECT_AST(m_node.arg[2].nodes.begin(), ARG3);
/// @TODO Expect sizes

TEST_F(ast, nullary) {
#define N(TYPE, ID)          \
    test(m_ast.make_##ID()); \
    EXPECT_NODE0(Node_##ID);
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, unary) {
#define U(TYPE, ID)                   \
    test(m_ast.make_##ID(n_t_hello)); \
    EXPECT_NODE1(Node_##ID, &n_t_hello);
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, binary) {
#define B(TYPE, ID)                              \
    test(m_ast.make_##ID(n_t_hello, n_t_hello)); \
    EXPECT_NODE2(Node_##ID, &n_t_hello, &n_t_hello);
#include "nkl/lang/nodes.inl"
}

TEST_F(ast, if) {
    test(m_ast.make_if(n_t_hello, n_t_hello, n_t_hello));
    EXPECT_NODE3(Node_if, &n_t_hello, &n_t_hello, &n_t_hello);
}
