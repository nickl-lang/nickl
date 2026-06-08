#include "ast_parser.h"

#include "ast_tokens.h"
#include "nickl_impl.h"
#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/lexer.h"
#include "nodes.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/stream.h"
#include "ntk/string.h"

NK_LOG_USE_SCOPE(ast_parser);

#define TRY(EXPR)         \
    do {                  \
        if (!(EXPR)) {    \
            return false; \
        }                 \
    } while (0)

typedef struct {
    NklState const nkl;
    NkAtom const file;
    NkString const text;
    NklTokenArray const tokens;
    NkArena *const arena;

    char const **token_names;

    NklAstNodeDynArray nodes;

    NklToken const *cur_token;
} ParserState;

static void vreportError(ParserState *p, char const *fmt, va_list ap) {
    nickl_vreportError(
        p->nkl,
        (NklSourceLocation){
            .file = nk_atom2s(p->file),
            .lin = p->cur_token->lin,
            .col = p->cur_token->col,
            .len = p->cur_token->len,
        },
        fmt,
        ap);
}

NK_PRINTF_LIKE(2) static void reportError(ParserState *p, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreportError(p, fmt, ap);
    va_end(ap);
}

static bool on(ParserState const *p, u32 id) {
    return p->cur_token->id == id;
}

static void getToken(ParserState *p) {
    nk_assert(!on(p, NklToken_Eof));

    do {
        p->cur_token++;
    } while (on(p, NklToken_Newline));

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_printf(log, "next: %s \"", p->token_names ? p->token_names[p->cur_token->id] : "??TokenNameUnavailable??");
        nks_escape(log, nkl_getTokenStr(p->cur_token, p->text));
        nk_print(log, "\"");
    }
}

static bool accept(ParserState *p, u32 id) {
    if (on(p, id)) {
        NK_LOG_STREAM_DBG {
            NkStream log = nk_log_getStream();
            nk_printf(
                log, "accept: %s \"", p->token_names ? p->token_names[p->cur_token->id] : "??TokenNameUnavailable??");
            nks_escape(log, nkl_getTokenStr(p->cur_token, p->text));
            nk_print(log, "\"");
        }

        getToken(p);
        return true;
    }
    return false;
}

static bool expect(ParserState *p, u32 id) {
    if (!accept(p, id)) {
        NkString const token_str = nkl_getTokenStr(p->cur_token, p->text);
        reportError(
            p,
            "expected `%s` before `" NKS_FMT "`",
            p->token_names ? p->token_names[id] : "??TokenNameUnavailable??",
            NKS_ARG(token_str));
        return false;
    }
    return true;
}

static bool parseNode(ParserState *p);

static bool parseNodeList(ParserState *p, NklAstNode *node) {
    u32 const old_count = p->nodes.size;
    while (!on(p, NklToken_Eof) && !on(p, NklAstToken_RBraket) && !on(p, NklAstToken_RParen)) {
        TRY(parseNode(p));
        node->arity++;
    }

    node->total_children = p->nodes.size - old_count;
    return true;
}

static NklAstNode *pushNode(ParserState *p) {
    nkda_append(
        &p->nodes,
        ((NklAstNode){
            .token_idx = p->cur_token - p->tokens.data,
        }));
    return &NKS_LAST(p->nodes);
}

static bool parseNode(ParserState *p) {
    NklAstNode *node = pushNode(p);

    if (accept(p, NklAstToken_LParen)) {
        if (!accept(p, NklAstToken_RParen)) {
            if (on(p, NklToken_Id)) {
                NkString const token_str = nkl_getTokenStr(p->cur_token, p->text);
                node->id = nk_s2atom(token_str);
                getToken(p);
            } else if (on(p, NklToken_String)) {
                char const *data = p->text.data + p->cur_token->pos + 1;
                usize const len = p->cur_token->len - 2;
                node->id = nk_s2atom((NkString){data, len});
                getToken(p);
            } else {
                NkString const token_str = nkl_getTokenStr(p->cur_token, p->text);
                reportError(p, "unexpected token `" NKS_FMT "`", NKS_ARG(token_str));
                return false;
            }
            node->token_idx++;
            TRY(parseNodeList(p, node));
            TRY(expect(p, NklAstToken_RParen));
        }
    }

    else if (accept(p, NklAstToken_LBraket)) {
        node->id = n_list;
        TRY(parseNodeList(p, node));
        TRY(expect(p, NklAstToken_RBraket));
    }

    else if (accept(p, NklToken_Id)) {
        node->id = n_id;
    }

    else if (accept(p, NklToken_Int)) {
        node->id = n_int;
    } else if (accept(p, NklToken_Float)) {
        node->id = n_float;
    }

    else if (accept(p, NklToken_String)) {
        node->id = n_string;
    } else if (accept(p, NklToken_EscapedString)) {
        node->id = n_escaped_string;
    }

    else {
        NkString const token_str = nkl_getTokenStr(p->cur_token, p->text);
        reportError(p, "unexpected token `" NKS_FMT "`", NKS_ARG(token_str));
        return false;
    }

    return true;
}

static bool parse(ParserState *p) {
    nk_assert(p->tokens.size && NKS_LAST(p->tokens).id == NklToken_Eof && "ill-formed token stream");

    if (on(p, NklToken_Newline)) {
        getToken(p);
    }

    NklAstNode *node = pushNode(p);
    node->id = n_list;

    TRY(parseNodeList(p, node));

    if (!on(p, NklToken_Eof)) {
        NkString const token_str = nkl_getTokenStr(p->cur_token, p->text);
        reportError(p, "unexpected token `" NKS_FMT "`", NKS_ARG(token_str));
        return false;
    }

    return true;
}

bool nkl_ast_parse(NklAstParserArgs const *args, NklAstNodeArray *out_nodes) {
    NK_LOG_TRC("%s", __func__);

    NklState const nkl = args->nkl;
    NkAtom const file = args->file;

    NkString text;
    if (!nickl_getText(nkl, file, &text)) {
        return false;
    }

    NklTokenArray tokens;
    TRY(nickl_getTokensAst(nkl, file, &tokens));

    ParserState p = {
        .nkl = nkl,
        .file = file,
        .text = text,
        .tokens = tokens,
        .arena = &nkl->arena,
        .token_names = args->token_names,
        .cur_token = p.tokens.data,
        .nodes = {.alloc = nk_arena_getAllocator(&nkl->arena)},
    };
    nkda_reserve(&p.nodes, 1000);

    TRY(parse(&p));

    NK_LOG_STREAM_INF {
        NkStream log = nk_log_getStream();
        nk_print(log, "AST:");
        nkl_ast_inspect(
            (NklSource){
                .file = 0,
                .text = text,
                .tokens = tokens,
                .nodes = {NKS_INIT(p.nodes)},
            },
            log);
    }

    *out_nodes = (NklAstNodeArray){NKS_INIT(p.nodes)};
    return true;
}
