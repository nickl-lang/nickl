#include "nkl/lang/ast.h"

#include <algorithm>
#include <iterator>
#include <new>

#include <gtest/gtest.h>

#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/utils.hpp"

namespace {

NK_LOG_USE_SCOPE(test);

class ast : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT(NkLoggerOptions{});

        m_ast = nkl_ast_create();
        m_arena = nk_create_arena();
    }

    void TearDown() override {
        nk_free_arena(m_arena);
        nkl_ast_free(m_ast);
    }

protected:
    void inspect(NklNodeArray node) {
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };

        nkl_ast_inspect(node.data, sb);
        auto str = nksb_concat(sb);

        NK_LOG_INF("ast:%.*s", str.size, str.data);
    }

    NklToken const *mkt(char const *text) {
        return new (nk_allocate(m_arena, sizeof(NklToken))) NklToken{
            .text = text ? cs2s(text) : nkstr{},
            .pos = 0,
            .lin = 0,
            .col = 0,
            .id = 0,
        };
    }

    NklNodeArray mkn(char const *id, char const *text) {
        return mkn(id, text, {}, {}, {});
    }

    NklNodeArray mkn(char const *id, NklNodeArray arg0) {
        return mkn(id, arg0, {}, {});
    }

    NklNodeArray mkn(char const *id, NklNodeArray arg0, NklNodeArray arg1) {
        return mkn(id, arg0, arg1, {});
    }

    NklNodeArray mkn(char const *id, NklNodeArray arg0, NklNodeArray arg1, NklNodeArray arg2) {
        return mkn(id, {}, arg0, arg1, arg2);
    }

    NklNodeArray mkn(
        char const *id,
        char const *text,
        NklNodeArray arg0,
        NklNodeArray arg1,
        NklNodeArray arg2) {
        return mkn(NklNode{
            .args{arg0, arg1, arg2},
            .token = mkt(text),
            .id = cs2nkid(id),
        });
    }

    NklNodeArray mkn(NklNode const &node) {
        return {nkl_ast_pushNode(m_ast, node), 1};
    }

    NklNodeArray mkar(std::vector<NklNodeArray> const &ar) {
        std::vector<NklNode> nodes;
        std::transform(ar.begin(), ar.end(), std::back_inserter(nodes), [](NklNodeArray ar) {
            return ar.data[0];
        });
        return nkl_ast_pushNodeAr(m_ast, {nodes.data(), nodes.size()});
    }

protected:
    NklAst m_ast;
    NkAllocator *m_arena;
};

} // namespace

TEST_F(ast, empty) {
    inspect({});
}

TEST_F(ast, basic) {
    auto n_const2 = mkn("int", "2");
    auto n_add = mkn("add", n_const2, n_const2);
    inspect(n_add);
}

TEST_F(ast, fn_def) {
    auto n_u32 = mkn("u32", "u32");
    auto n_lhs = mkn("id", "lhs");
    auto n_rhs = mkn("id", "rhs");

    auto n_fn_def =
        mkn("const_def",
            mkn("id", "add"),
            mkn("fn",
                mkar({
                    mkn("param", n_lhs, n_u32),
                    mkn("param", n_rhs, n_u32),
                }),
                n_u32,
                mkn("block",
                    mkar({
                        mkn("return", mkn("add", n_lhs, n_rhs)),
                    }))));

    inspect(n_fn_def);
}
