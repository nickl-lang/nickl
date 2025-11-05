#include "nickl_impl.h"
#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/types.h"
#include "nodes.h"
#include "ntk/arena.h"
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
    NklType type;
    bool is_comptime;
} AstNodeExt;

typedef NkSlice(AstNodeExt) AstNodeExtArray;

typedef struct {
    NklState nkl;
    NklModule mod;

    NkAtom file;
    NkString text;
    NklTokenArray tokens;
    NklAstNodeArray nodes;

    AstNodeExtArray nodes_ext;
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

static NkString parseString(CompilerCtx *ctx, NkArena *arena, NklAstNode const *node) {
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

static bool typecheck(CompilerCtx *ctx, NklAstNode const *node) {
    u32 const node_idx = nodeIdx(ctx->nodes, node);
    NK_LOG_DBG("Typechecking node %5u | %s", node_idx, nk_atom2cs(node->id));

    AstNodeExt *node_ext = &ctx->nodes_ext.data[node_idx];

    AstNodeIterator it = nodeIterate(ctx->nodes, node);

    switch (node->id) {
        case 0: {
            *node_ext = (AstNodeExt){
                .type = nkl_type_getVoid(ctx->nkl),
                .is_comptime = true,
            };
            return true;
        }

        case n_list: {
            for (u32 i = 0; i < node->arity; i++) {
                NklAstNode const *child_node = nextNode(&it);

                TRY(typecheck(ctx, child_node), false);

                AstNodeExt const *child_node_ext = &ctx->nodes_ext.data[nodeIdx(ctx->nodes, child_node)];
                *node_ext = *child_node_ext;
            }
            return true;
        }

        default: {
            reportError(ctx, node, "unknown AST node `%s`", nk_atom2cs(node->id));
            return false;
        }
    }

    nk_assert(!"unreachable");
    return false;
}

static void compile(CompilerCtx *ctx, NklAstNode const *node) {
    u32 const node_idx = nodeIdx(ctx->nodes, node);
    NK_LOG_DBG("Compiling node %5u | %s", node_idx, nk_atom2cs(node->id));

    AstNodeExt const *node_ext = &ctx->nodes_ext.data[node_idx];

    AstNodeIterator it = nodeIterate(ctx->nodes, node);

    switch (node->id) {
        case 0: {
            break;
        }

        case n_list: {
            for (u32 i = 0; i < node->arity; i++) {
                compile(ctx, nextNode(&it));
            }
            break;
        }

        default: {
            nk_assert(!"unreachable");
            break;
        }
    }
}

bool nickl_compile(NklCompileArgs const *args) {
    NK_LOG_TRC("%s", __func__);

    bool ok = true;
    NK_PROF_FUNC() {
        if (args->nodes.size) {
            NkArena *scratch = nk_arena_getScratch(NULL);
            NK_ARENA_SCOPE(scratch) {
                CompilerCtx ctx = {
                    .nkl = args->mod->com->nkl,
                    .mod = args->mod,

                    .file = args->file,
                    .text = args->text,
                    .tokens = args->tokens,
                    .nodes = args->nodes,

                    .nodes_ext =
                        {
                            .data = nk_arena_allocTn(scratch, AstNodeExt, args->nodes.size),
                            .size = args->nodes.size,
                        },
                };
                NKS_ZERO(ctx.nodes_ext);

                NklAstNode const *root = &NKS_FIRST(args->nodes);
                ok = typecheck(&ctx, root);
                if (ok) {
                    compile(&ctx, root);
                }
            }
        }
    }
    return ok;
}
