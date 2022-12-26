#include "nkl/lang/ast.h"

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

        NK_LOG_INF("ast:\n\n%.*s", str.size, str.data);
    }

    NklToken const *mkt(char const *text) {
        return new (nk_allocate(m_arena, sizeof(NklToken))) NklToken{
            .text = cs2s(text),
            .pos = 0,
            .lin = 0,
            .col = 0,
            .id = 0,
        };
    }

    NklNodeArray mkn(NklNode const &node) {
        return {nkl_ast_pushNode(m_ast, node), 1};
    }

    NklNodeArray mkar(std::vector<NklNode> const &ar) {
        return nkl_ast_pushNodeAr(m_ast, {ar.data(), ar.size()});
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
    auto n_const2 = mkn(NklNode{
        .args{},
        .token = mkt("2"),
        .id = cs2nkid("int"),
    });

    auto n_add = mkn(NklNode{
        .args{
            n_const2,
            n_const2,
            {},
        },
        .token = mkt("+"),
        .id = cs2nkid("add"),
    });

    inspect(n_add);
}

TEST_F(ast, fn_def) {
    auto n_u32 = mkn(NklNode{
        .args{},
        .token = mkt("u32"),
        .id = cs2nkid("u32"),
    });

    auto n_fn_def = mkn(NklNode{
        .args{
            mkn(NklNode{
                .args{},
                .token = mkt("add"),
                .id = cs2nkid("id"),
            }),
            mkn(NklNode{
                .args{
                    mkar({
                        NklNode{
                            .args{
                                mkn(NklNode{
                                    .args{},
                                    .token = mkt("lhs"),
                                    .id = cs2nkid("id"),
                                }),
                                n_u32,
                                {},
                            },
                            .token = mkt("lhs"),
                            .id = cs2nkid("param"),
                        },
                        NklNode{
                            .args{
                                mkn(NklNode{
                                    .args{},
                                    .token = mkt("rhs"),
                                    .id = cs2nkid("id"),
                                }),
                                n_u32,
                                {},
                            },
                            .token = mkt("rhs"),
                            .id = cs2nkid("param"),
                        },
                    }),
                    n_u32,
                    mkn(NklNode{
                        .args{
                            mkar({
                                NklNode{
                                    .args{
                                        mkn(NklNode{
                                            .args{
                                                mkn(NklNode{
                                                    .args{},
                                                    .token = mkt("lhs"),
                                                    .id = cs2nkid("id"),
                                                }),
                                                mkn(NklNode{
                                                    .args{},
                                                    .token = mkt("rhs"),
                                                    .id = cs2nkid("id"),
                                                }),
                                                {},
                                            },
                                            .token = mkt("+"),
                                            .id = cs2nkid("add"),
                                        }),
                                        {},
                                        {},
                                    },
                                    .token = mkt("return"),
                                    .id = cs2nkid("return"),
                                },
                            }),
                            {},
                            {},
                        },
                        .token = mkt("{"),
                        .id = cs2nkid("block"),
                    }),
                },
                .token = mkt("("),
                .id = cs2nkid("fn"),
            }),
            {},
        },
        .token = mkt("::"),
        .id = cs2nkid("const_def"),
    });

    inspect(n_fn_def);
}
