#include "nkl/lang/ast.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nkl/core/test/ast_utils.hpp"

using namespace nkl;
using namespace nkl::test;

class ast : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_ast.init();
    }

    void TearDown() override {
        m_ast.deinit();
    }

protected:
    enum ETestNodeId {
        TestNode_t_hello = 42,
    };

    Token const t_hello{cs2s("hello"), 1, 2, 3};
    Node const n_t_hello{{{&t_hello, {}}, {}, {}}, TestNode_t_hello};

    LangAst m_ast{};
};

TEST_F(ast, basic) {
    auto node = m_ast.make_if(n_t_hello, n_t_hello, n_t_hello);
    EXPECT_EQ(node.id, Node_if);
    EXPECT_TRUE(ast_equal(node.arg[0].nodes.begin(), &n_t_hello));
    EXPECT_TRUE(ast_equal(node.arg[1].nodes.begin(), &n_t_hello));
    EXPECT_TRUE(ast_equal(node.arg[2].nodes.begin(), &n_t_hello));
}
