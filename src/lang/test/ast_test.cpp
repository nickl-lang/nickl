#include "nkl/lang/ast.h"

#include <gtest/gtest.h>

#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/utils.hpp"

namespace {

NK_LOG_USE_SCOPE(test);

class ast : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT(NkLoggerOptions{});

        m_ast = nkl_ast_create();
    }

    void TearDown() override {
        nkl_ast_free(m_ast);
    }

protected:
    void inspect() {
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };

        nkl_ast_inspect(m_root, sb);
        auto str = nksb_concat(sb);

        NK_LOG_INF("ast:\n%.*s", str.size, str.data);
    }

protected:
    NklAst m_ast;
    NklNode const *m_root = nullptr;
};

} // namespace

TEST_F(ast, empty) {
    inspect();
}

TEST_F(ast, basic) {
    NklToken t_const2{.text = cs2s("2")};
    NklToken t_plus{.text = cs2s("+")};

    auto const2 =
        nkl_ast_pushNode(m_ast, NklNode{.args{}, .token = &t_const2, .id = cs2nkid("const")});
    auto plus = nkl_ast_pushNode(m_ast, NklNode{.args{}, .token = &t_plus, .id = cs2nkid("plus")});

    m_root = nkl_ast_pushNode(
        m_ast,
        NklNode{
            .args{
                {const2, 1},
                {const2, 1},
                {},
            },
            .token = &t_plus,
            .id = cs2nkid("add"),
        });

    inspect();
}
