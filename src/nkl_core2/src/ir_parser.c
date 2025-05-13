#include "nkl/core/ir_parser.h"

#include "nkb/ir.h"
#include "nkl/core/lexer.h"
#include "ntk/log.h"
#include "ntk/stream.h"

NK_LOG_USE_SCOPE(ir_parser);

#define TRY(EXPR)         \
    do {                  \
        if (!(EXPR)) {    \
            return false; \
        }                 \
    } while (0)

typedef struct {
    NkString const text;
    NklTokenArray const tokens;
    NkArena *const arena;

    NkString *const err_str;
    NklToken *const err_token;

    char const **token_names;

    NkIrSymbolDynArray *mod;

    u32 cur_token_idx;
} ParserState;

// TODO: Reuse some code between parsers?

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

    // TODO: Simplify stream logging
#ifdef ENABLE_LOGGING
    if (nk_log_check(NkLogLevel_Debug)) {
        NkStream log;
        NK_DEFER_LOOP(log = nk_log_streamOpen(NkLogLevel_Debug, _nk_log_scope), nk_log_streamClose(log)) {
            nk_printf(log, "next token: \"");
            nks_escape(log, nkl_getTokenStr(curToken(p), p->text));
            nk_printf(log, "\":%u", curToken(p)->id);
        }
    }
#endif // ENABLE_LOGGING
}

static bool accept(ParserState *p, u32 id) {
    if (on(p, id)) {
#ifdef ENABLE_LOGGING
        if (nk_log_check(NkLogLevel_Debug)) {
            NkStream log;
            NK_DEFER_LOOP(log = nk_log_streamOpen(NkLogLevel_Debug, _nk_log_scope), nk_log_streamClose(log)) {
                nk_printf(log, "accept \"");
                nks_escape(log, nkl_getTokenStr(curToken(p), p->text));
                nk_printf(log, "\":%u", curToken(p)->id);
            }
        }
#endif // ENABLE_LOGGING

        getToken(p);
        return true;
    }
    return false;
}

static bool expect(ParserState *p, u32 id) {
    if (!accept(p, id)) {
        NkString const token_str = nkl_getTokenStr(curToken(p), p->text);
        reportError(
            p,
            "expected `%s` before `" NKS_FMT "`",
            p->token_names ? p->token_names[id] : "??TokenNameUnavailable??",
            NKS_ARG(token_str));
        return false;
    }
    return true;
}

bool parse(ParserState *p) {
    reportError(p, "TODO: `parse` is not implemented");
    return false;
}

bool nkl_ir_parse(NklIrParserData const *data, NkIrSymbolDynArray *out_syms) {
    NK_LOG_TRC("%s", __func__);

    usize const start_idx = out_syms->size;

    ParserState p = {
        .text = data->text,
        .tokens = data->tokens,
        .arena = data->arena,

        .err_str = data->err_str,
        .err_token = data->err_token,

        .token_names = data->token_names,

        .mod = out_syms,

        .cur_token_idx = 0,
    };

    TRY(parse(&p));

    // TODO: Simplify stream logging
#ifdef ENABLE_LOGGING
    if (nk_log_check(NkLogLevel_Info)) {
        NkStream log;
        NK_DEFER_LOOP(log = nk_log_streamOpen(NkLogLevel_Info, _nk_log_scope), nk_log_streamClose(log)) {
            nk_printf(log, "IR:\n");
            NkIrModule const syms = {out_syms->data + start_idx, out_syms->size - start_idx};
            nkir_inspectModule(syms, log);
        }
    }
#endif // ENABLE_LOGGING

    return true;
}
