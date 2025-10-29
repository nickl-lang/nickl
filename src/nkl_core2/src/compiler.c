#include "nickl_impl.h"
#include "nkl/common/ast.h"
#include "nodes.h"
#include "ntk/common.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"

NK_LOG_USE_SCOPE(compiler);

#define TRY(EXPR, ...)          \
    do {                        \
        if (!(EXPR)) {          \
            return __VA_ARGS__; \
        }                       \
    } while (0)

static u32 nodeIdx(NklAstNodeArray nodes, NklAstNode const *node) {
    return node - nodes.data;
}

typedef struct {
    NklAstNodeArray nodes;
    u32 next_idx;
} AstNodeIterator;

static AstNodeIterator nodeIterate(NklAstNodeArray nodes, NklAstNode const *node) {
    return (AstNodeIterator){
        .nodes = nodes,
        .next_idx = nodeIdx(nodes, node) + 1,
    };
}

static NklAstNode const *nextNode(AstNodeIterator *it) {
    NklAstNode const *next_node = &it->nodes.data[it->next_idx];
    it->next_idx = nkl_ast_nextChild(it->nodes, it->next_idx);
    return next_node;
}

typedef struct {
} Interm;

typedef struct {
    NklCompiler com;
    NkAtom file;
    NklTokenArray tokens;
    NklAstNodeArray nodes;
} CompilerCtx;

static void vreportError(CompilerCtx *ctx, NklAstNode const *node, char const *fmt, va_list ap) {
    NklToken const *token = &ctx->tokens.data[node->token_idx];
    nickl_vreportError(
        ctx->com->nkl,
        (NklSourceLocation){
            .file = nk_atom2s(ctx->file),
            token->lin,
            token->col,
            token->len,
        },
        fmt,
        ap);
}

static NK_PRINTF_LIKE(3) void reportError(CompilerCtx *ctx, NklAstNode const *node, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreportError(ctx, node, fmt, ap);
    va_end(ap);
}

static bool compile(CompilerCtx *ctx, NklAstNode const *node) {
    NK_LOG_DBG("Compiling node %u | %s", nodeIdx(ctx->nodes, node), nk_atom2cs(node->id));

    AstNodeIterator node_it = nodeIterate(ctx->nodes, node);

    switch (node->id) {
        case n_null: {
            return true; // TODO: make void
        }

        case n_list: {
            for (u32 i = 0; i < node->arity; i++) {
                TRY(compile(ctx, nextNode(&node_it)), false);
            }
            return true; // TODO: make void
        }

        default: {
            reportError(ctx, node, "unknown AST node `%s`", nk_atom2cs(node->id));
            return false;
        }
    }

    nk_assert(!"unreachable");
    return false;
}

bool nickl_compile(NklCompileArgs const *args) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    CompilerCtx ctx = {
        .com = args->com,
        .file = args->file,
        .tokens = args->tokens,
        .nodes = args->nodes,
    };

    if (args->nodes.size) {
        TRY(compile(&ctx, &nks_first(args->nodes)), false);
    }

    return true;
}
