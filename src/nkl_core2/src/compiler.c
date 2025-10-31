#include "nickl_impl.h"
#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nodes.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/string_builder.h"

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
    NkArena scratch;

    NklModule mod;

    NkAtom file;
    NkString text;
    NklTokenArray tokens;
    NklAstNodeArray nodes;
} CompilerCtx;

static void vreportError(CompilerCtx *ctx, NklAstNode const *node, char const *fmt, va_list ap) {
    NklToken const *token = &ctx->tokens.data[node->token_idx];
    nickl_vreportError(
        ctx->mod->com->nkl,
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

static NkString getString(CompilerCtx *ctx, NklAstNode const *node, NkArena *arena) {
    nk_assert(node->id == n_string || node->id == n_escaped_string);

    NklToken const *token = &ctx->tokens.data[node->token_idx];

    NkString const token_str = nkl_getTokenStr(token, ctx->text);
    NkString const str = (NkString){token_str.data + 1, token_str.size - 2};

    if (node->id == n_string) {
        char const *cstr = nk_tprintf(arena, NKS_FMT, NKS_ARG(str));
        return (NkString){cstr, str.size};
    } else {
        NkStringBuilder sb = {.alloc = nk_arena_getAllocator(arena)};
        nks_unescape(nksb_getStream(&sb), str);
        if (NKS_LAST(sb)) {
            nksb_appendNull(&sb);
        }

        return (NkString){sb.data, sb.size - 1};
    }
}

static bool compile(CompilerCtx *ctx, NklAstNode const *node) {
    NK_LOG_DBG("Compiling node %5u | %s", nodeIdx(ctx->nodes, node), nk_atom2cs(node->id));

    AstNodeIterator it = nodeIterate(ctx->nodes, node);

    switch (node->id) {
        case 0: {
            return true; // TODO: make void
        }

        case n_list: {
            for (u32 i = 0; i < node->arity; i++) {
                TRY(compile(ctx, nextNode(&it)), false);
            }
            return true; // TODO: make void
        }

        case n_extern: {
            NklAstNode const *lib_node = nextNode(&it);
            NklAstNode const *decl_node = nextNode(&it);

            NkAtom lib = 0;
            if (lib_node->id) {
                lib = nk_s2atom(getString(ctx, lib_node, &ctx->scratch));
            }

            reportError(ctx, node, "extern compilation unfinished");
            return false;
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
    NK_LOG_TRC("%s", __func__);

    bool ok = true;
    NK_PROF_FUNC() {
        if (args->nodes.size) {
            CompilerCtx ctx = {
                .mod = args->mod,
                .file = args->file,
                .text = args->text,
                .tokens = args->tokens,
                .nodes = args->nodes,
            };

            ok = compile(&ctx, &NKS_FIRST(args->nodes));

            // TODO: Reuse scratch arena for other compiles
            nk_arena_free(&ctx.scratch);
        }
    }

    return ok;
}
