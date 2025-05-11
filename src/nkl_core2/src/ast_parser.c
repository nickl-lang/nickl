#include "nkl/core/ast_parser.h"

#include "ast_tokens.h"
#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/lexer.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(parser);

#define LOG_TOKEN(ID) "(%s, \"%s\")", "<TODO:TokenId>", "<TODO:TokenName>"

#define TRY(EXPR)         \
    do {                  \
        if (!(EXPR)) {    \
            return false; \
        }                 \
    } while (0)

typedef struct {
    NkString const text;
    NkArena *const arena;
    NklTokenArray const tokens;
    NkString *const err_str;
    NklToken *const err_token;

    NklAstNodeDynArray nodes;

    u32 cur_token_idx;
} ParserState;

static NklToken const *curToken(ParserState const *p) {
    return &p->tokens.data[p->cur_token_idx];
}

static i32 vreportError(ParserState *p, char const *fmt, va_list ap) {
    i32 res = 0;
    if (p->err_str) {
        *p->err_str = nk_vtsprintf(p->arena, fmt, ap);
        *p->err_token = *curToken(p);
    }
    return res;
}

NK_PRINTF_LIKE(2) static i32 reportError(ParserState *p, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    i32 res = vreportError(p, fmt, ap);
    va_end(ap);

    return res;
}

static bool on(ParserState const *p, u32 id) {
    return curToken(p)->id == id;
}

static void getToken(ParserState *p) {
    nk_assert(!on(p, NklToken_Eof));
    p->cur_token_idx++;
    NK_LOG_DBG("next token: " LOG_TOKEN(curToken(p)->id));
}

static bool accept(ParserState *p, u32 id) {
    if (on(p, id)) {
        NK_LOG_DBG("accept" LOG_TOKEN(id));
        getToken(p);
        return true;
    }
    return false;
}

static bool expect(ParserState *p, u32 id) {
    if (!accept(p, id)) {
        NkString const token_str = nkl_getTokenStr(curToken(p), p->text);
        reportError(p, "expected `%s` before `" NKS_FMT "`", "<TODO:TokenName>", NKS_ARG(token_str));
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
            .id = 0,
            .token_idx = p->cur_token_idx,
            .total_children = 0,
            .arity = 0,
        }));
    return &nk_slice_last(p->nodes);
}

static bool parseNode(ParserState *p) {
    NklAstNode *node = pushNode(p);

    if (accept(p, NklAstToken_LParen)) {
        if (!accept(p, NklAstToken_RParen)) {
            if (on(p, NklToken_Id)) {
                NkString const token_str = nkl_getTokenStr(curToken(p), p->text);
                node->id = nk_s2atom(token_str);
                getToken(p);
            } else if (on(p, NklToken_String)) {
                char const *data = p->text.data + curToken(p)->pos + 1;
                usize const len = curToken(p)->len - 2;
                node->id = nk_s2atom((NkString){data, len});
                getToken(p);
            } else {
                NkString const token_str = nkl_getTokenStr(curToken(p), p->text);
                reportError(p, "unexpected token `" NKS_FMT "`", NKS_ARG(token_str));
                return false;
            }
            node->token_idx++;
            TRY(parseNodeList(p, node));
            TRY(expect(p, NklAstToken_RParen));
        }
    }

    else if (accept(p, NklAstToken_LBraket)) {
        node->id = nk_cs2atom("list");
        TRY(parseNodeList(p, node));
        TRY(expect(p, NklAstToken_RBraket));
    }

    else if (accept(p, NklToken_Id)) {
        node->id = nk_cs2atom("id");
    }

    else if (accept(p, NklToken_Int)) {
        node->id = nk_cs2atom("int");
    } else if (accept(p, NklToken_Float)) {
        node->id = nk_cs2atom("float");
    }

    else if (accept(p, NklToken_String)) {
        node->id = nk_cs2atom("string");
    } else if (accept(p, NklToken_EscapedString)) {
        node->id = nk_cs2atom("escaped_string");
    }

    else {
        NkString const token_str = nkl_getTokenStr(curToken(p), p->text);
        reportError(p, "unexpected token `" NKS_FMT "`", NKS_ARG(token_str));
        return false;
    }

    return true;
}

static bool parse(ParserState *p) {
    nk_assert(p->tokens.size && nk_slice_last(p->tokens).id == NklToken_Eof && "ill-formed token stream");

    NklAstNode *node = pushNode(p);
    node->id = nk_cs2atom("list");

    TRY(parseNodeList(p, node));

    if (!on(p, NklToken_Eof)) {
        NkString const token_str = nkl_getTokenStr(curToken(p), p->text);
        reportError(p, "unexpected token `" NKS_FMT "`", NKS_ARG(token_str));
        return false;
    }

    return true;
}

bool nkl_ast_parse(NklAstParserData const *data, NklAstNodeArray *out_nodes) {
    NK_LOG_TRC("%s", __func__);

    ParserState p = {
        .text = data->text,
        .arena = data->arena,
        .tokens = data->tokens,
        .err_str = data->err_str,
        .err_token = data->err_token,

        .nodes = {NKDA_INIT(nk_arena_getAllocator(data->arena))},

        .cur_token_idx = 0,
    };
    nkda_reserve(&p.nodes, 1000);

    TRY(parse(&p));

#ifdef ENABLE_LOGGING
    {
        NkStringBuilder sb = {0};
        nkl_ast_inspect(
            (NklSource){
                .file = 0,
                .text = data->text,
                .tokens = data->tokens,
                .nodes = {NK_SLICE_INIT(p.nodes)},
            },
            nksb_getStream(&sb));
        NK_LOG_INF("AST:" NKS_FMT, NKS_ARG(sb));
        nksb_free(&sb); // TODO: Use scratch arena
    }
#endif // ENABLE_LOGGING

    *out_nodes = (NklAstNodeArray){NK_SLICE_INIT(p.nodes)};
    return true;
}
