#include "nkl/lang/compiler.h"

#include <algorithm>
#include <iterator>
#include <new>

#include <gtest/gtest.h>

#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/utils.hpp"
#include "nkl/lang/ast.h"

namespace {

NK_LOG_USE_SCOPE(test);

class compiler : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT(NkLoggerOptions{});

        m_ast = nkl_ast_create();
        m_compiler = nkl_compiler_create();
        m_arena = nk_create_arena();
    }

    void TearDown() override {
        nk_free_arena(m_arena);
        nkl_compiler_free(m_compiler);
        nkl_ast_free(m_ast);
    }

protected:
    void inspect(NklAstNodeArray node) {
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };

        nkl_ast_inspect(node.data, sb);
        auto str = nksb_concat(sb);

        NK_LOG_INF("ast:%.*s\n", str.size, str.data);
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

    NklAstNodeArray mkn(char const *id, char const *text) {
        return mkn(id, text, {}, {}, {});
    }

    NklAstNodeArray mkn(char const *id, NklAstNodeArray arg0) {
        return mkn(id, arg0, {}, {});
    }

    NklAstNodeArray mkn(char const *id, NklAstNodeArray arg0, NklAstNodeArray arg1) {
        return mkn(id, arg0, arg1, {});
    }

    NklAstNodeArray mkn(
        char const *id,
        NklAstNodeArray arg0,
        NklAstNodeArray arg1,
        NklAstNodeArray arg2) {
        return mkn(id, {}, arg0, arg1, arg2);
    }

    NklAstNodeArray mkn(
        char const *id,
        char const *text,
        NklAstNodeArray arg0,
        NklAstNodeArray arg1,
        NklAstNodeArray arg2) {
        return mkn(NklAstNode_T{
            .args{arg0, arg1, arg2},
            .token = mkt(text),
            .id = cs2nkid(id),
        });
    }

    NklAstNodeArray mkn(NklAstNode_T node) {
        return {nkl_ast_pushNode(m_ast, &node), 1};
    }

    NklAstNodeArray mkar(std::vector<NklAstNodeArray> const &ar) {
        std::vector<NklAstNode_T> nodes;
        std::transform(ar.begin(), ar.end(), std::back_inserter(nodes), [](NklAstNodeArray ar) {
            return ar.data[0];
        });
        return nkl_ast_pushNodeAr(m_ast, {nodes.data(), nodes.size()});
    }

protected:
    NklAst m_ast;
    NklCompiler m_compiler;
    NkAllocator *m_arena;
};

} // namespace

TEST_F(compiler, empty) {
    auto n_empty = NklAstNodeArray{};
    inspect(n_empty);

    nkl_compiler_run(m_compiler, n_empty.data);
}

TEST_F(compiler, basic) {
    auto n_const2 = mkn("int", "2");
    auto n_add = mkn("add", n_const2, n_const2);

    inspect(n_add);

    nkl_compiler_run(m_compiler, n_add.data);
}

TEST_F(compiler, fn) {
    auto n_u32 = mkn("u32", "u32");
    auto n_lhs = mkn("id", "lhs");
    auto n_rhs = mkn("id", "rhs");

    auto n_root =
        mkn("block",
            mkar({
                mkn("const_decl",
                    mkn("id", "add"),
                    mkn("fn",
                        mkar({
                            mkn("param", n_lhs, n_u32),
                            mkn("param", n_rhs, n_u32),
                        }),
                        n_u32,
                        mkn("block", mkn("return", mkn("add", n_lhs, n_rhs))))),
                mkn("call",
                    mkn("id", "add"),
                    mkar({
                        mkn("int", "4"),
                        mkn("int", "5"),
                    })),
            }));

    inspect(n_root);

    nkl_compiler_run(m_compiler, n_root.data);
}

TEST_F(compiler, native_puts) {
    auto n_root =
        mkn("block",
            mkar({
                mkn("const_decl",
                    mkn("id", "puts"),
                    mkn("tagged",
                        mkn("tag", "#foreign"),
                        mkn("tuple", mkn("string", "")),
                        mkn("fn",
                            mkn("param", mkn("id", "str"), mkn("ptr_type", mkn("u8", "u8"))),
                            mkn("void", "void")))),
                mkn("call", mkn("id", "puts"), mkn("string", "Hello, World!")),
            }));

    inspect(n_root);

    nkl_compiler_run(m_compiler, n_root.data);
}
