#include "nkl/core/ir_parser.h"

#include <stdlib.h>

#include "ir_tokens.h"
#include "nkb/ir.h"
#include "nkb/types.h"
#include "nkl/common/token.h"
#include "nkl/core/lexer.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(ir_parser);

#define TRY(STMT)            \
    STMT;                    \
    if (p->error_occurred) { \
        return ret;          \
    } else                   \
        _NK_NOP

#define EXPECT(ID) TRY(expect(p, (ID)))

#define ERROR(...)               \
    reportError(p, __VA_ARGS__); \
    return ret

typedef struct {
    NkString const text;
    NklTokenArray const tokens;
    NkArena *const arena;

    NkString *const err_str;
    NklToken *const err_token;
    bool error_occurred;

    char const **token_names;

    NkIrSymbolDynArray *ir;

    NklToken const *cur_token;

    NkArena scratch;

#define X(TYPE, VALUE_TYPE) NkIrType NK_CAT(_cached_, TYPE);
    NKIR_NUMERIC_ITERATE(X)
#undef X
    NkIrType _cached_void;

    NkIrParamArray proc_params;
    NkIrType proc_ret_t;
} ParserState;

static NkIrType allocNumericType(ParserState *p, NkIrNumericValueType value_type) {
    NkIrType_T *type = nk_arena_allocT(p->arena, NkIrType_T);
    *type = (NkIrType_T){
        .num = value_type,
        .size = NKIR_NUMERIC_TYPE_SIZE(value_type),
        .align = NKIR_NUMERIC_TYPE_SIZE(value_type),
        .id = 0,
        .kind = NkIrType_Numeric,
    };
    return type;
}

static NkIrType allocVoidType(ParserState *p) {
    NkIrType_T *type = nk_arena_allocT(p->arena, NkIrType_T);
    *type = (NkIrType_T){0};
    return type;
}

#define CACHED_TYPE(NAME, EXPR)                        \
    NkIrType NK_CAT(get_, NAME)(ParserState * p) {     \
        NkIrType *cached = &p->NK_CAT(_cached_, NAME); \
        return *cached ? *cached : (*cached = (EXPR)); \
    };

#define X(TYPE, VALUE_TYPE) CACHED_TYPE(TYPE, allocNumericType(p, VALUE_TYPE))
NKIR_NUMERIC_ITERATE(X)
#undef X

CACHED_TYPE(void, allocVoidType(p));

#undef CACHED_TYPE

// TODO: Reuse some code between parsers?

typedef struct {
} Void;
Void const ret;

static void vreportError(ParserState *p, char const *fmt, va_list ap) {
    if (p->err_str) {
        *p->err_str = nk_vtsprintf(p->arena, fmt, ap);
        *p->err_token = *p->cur_token;
        p->error_occurred = true;
    }
}

static NK_PRINTF_LIKE(2) void reportError(ParserState *p, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreportError(p, fmt, ap);
    va_end(ap);
}

static bool on(ParserState const *p, u32 id) {
    return p->cur_token->id == id;
}

static NkString tokenStr(ParserState *p) {
    return nkl_getTokenStr(p->cur_token, p->text);
}

static void getToken(ParserState *p) {
    nk_assert(!on(p, NklToken_Eof));
    p->cur_token++;

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_printf(log, "next token: \"");
        nks_escape(log, tokenStr(p));
        nk_printf(log, "\":%u", p->cur_token->id);
    }
}

static bool accept(ParserState *p, u32 id) {
    if (on(p, id)) {
        NK_LOG_STREAM_DBG {
            NkStream log = nk_log_getStream();
            nk_printf(log, "accept \"");
            nks_escape(log, tokenStr(p));
            nk_printf(log, "\":%u", p->cur_token->id);
        }

        getToken(p);
        return true;
    }
    return false;
}

static NklToken const *expect(ParserState *p, u32 id) {
    NklToken const *ret = p->cur_token;
    if (!accept(p, id)) {
        NkString const str = tokenStr(p);
        ERROR(
            "expected `%s` before `" NKS_FMT "`",
            p->token_names ? p->token_names[id] : "??TokenNameUnavailable??",
            NKS_ARG(str));
    }
    return ret;
}

static NkIrType parseType(ParserState *p) {
    NkIrType ret = NULL;

    if (0) {
    }
#define X(TYPE, VALUE_TYPE)                          \
    else if (accept(p, NK_CAT(NklIrToken_, TYPE))) { \
        ret = NK_CAT(get_, TYPE)(p);                 \
    }
    NKIR_NUMERIC_ITERATE(X)
#undef X

    else if (accept(p, NklIrToken_void)) {
        ret = get_void(p);
    }

    else {
        NkString const str = tokenStr(p);
        ERROR("unexpected token `" NKS_FMT "`", NKS_ARG(str));
    }

    return ret;
}

static NkIrImm parseImm(ParserState *p, NkIrNumericValueType value_type) {
    NkIrImm ret = {0};

    if (NKIR_NUMERIC_IS_INT(value_type)) {
        if (!on(p, NklToken_Int)) {
            ERROR("integer constant expected");
        }
    }

    NkString const str = tokenStr(p);
    char const *cstr = nk_tprintf(&p->scratch, NKS_FMT, NKS_ARG(str));
    getToken(p);

    char *endptr = NULL;

    switch (value_type) {
        case Int8:
            ret.i8 = strtol(cstr, &endptr, 10);
            break;
        case Uint8:
            ret.u8 = strtoul(cstr, &endptr, 10);
            break;
        case Int16:
            ret.i16 = strtol(cstr, &endptr, 10);
            break;
        case Uint16:
            ret.u16 = strtoul(cstr, &endptr, 10);
            break;
        case Int32:
            ret.i32 = strtol(cstr, &endptr, 10);
            break;
        case Uint32:
            ret.u32 = strtoul(cstr, &endptr, 10);
            break;
        case Int64:
            ret.i64 = strtoll(cstr, &endptr, 10);
            break;
        case Uint64:
            ret.u64 = strtoull(cstr, &endptr, 10);
            break;
        case Float32:
            ret.f32 = strtof(cstr, &endptr);
            break;
        case Float64:
            ret.f64 = strtod(cstr, &endptr);
            break;
    }

    if (endptr != cstr + str.size) {
        ERROR("failed to parse numeric constant");
    }

    return ret;
}

static NkIrRef parseLocal(ParserState *p, NkIrType type_opt) {
    NkIrRef ret = {0};

    if (!on(p, NklIrToken_PercentTag)) {
        NkString const str = tokenStr(p);
        ERROR("unexpected token `" NKS_FMT "`", NKS_ARG(str));
    }

    NkString const str = tokenStr(p);
    getToken(p);

    NkAtom const local = nk_s2atom((NkString){str.data + 1, str.size - 1});

    NkIrType type = type_opt;

    if (!type) {
        for (usize i = 0; i < p->proc_params.size; i++) {
            NkIrParam const *param = &p->proc_params.data[i];
            if (local == param->name) {
                type = param->type;
                break;
            }
        }
    }

    if (!type) {
        EXPECT(NklIrToken_Colon);
        TRY(type = parseType(p));
    } else if (accept(p, NklIrToken_Colon)) {
        TRY(type = parseType(p));
    }

    return nkir_makeRefLocal(local, type);
}

static NkIrRef parseRef(ParserState *p, NkIrType type_opt) {
    NkIrRef ret = {0};

    if (on(p, NklToken_Int)) {
        bool const type_is_numeric = (type_opt && type_opt->kind == NkIrType_Numeric);
        TRY(NkIrImm imm = parseImm(p, type_is_numeric ? type_opt->num : Int64));
        ret = nkir_makeRefImm(imm, type_is_numeric ? type_opt : get_i64(p));
    } else if (on(p, NklToken_Float)) {
        bool const type_is_numeric = type_opt && type_opt->kind == NkIrType_Numeric;
        TRY(NkIrImm imm = parseImm(p, type_is_numeric ? type_opt->num : Float64));
        ret = nkir_makeRefImm(imm, type_is_numeric ? type_opt : get_f64(p));
    }

    else if (on(p, NklIrToken_PercentTag)) {
        TRY(ret = parseLocal(p, type_opt));
    }

    else if (on(p, NklToken_Id)) {
        NkString const str = tokenStr(p);
        getToken(p);

        NkAtom const id = nk_s2atom(str);

        ret = nkir_makeRefGlobal(id, NULL);
    }

    else {
        NkString const str = tokenStr(p);
        ERROR("unexpected token `" NKS_FMT "`", NKS_ARG(str));
    }

    if (!ret.type) {
        EXPECT(NklIrToken_Colon);
        TRY(ret.type = parseType(p));
    } else if (accept(p, NklIrToken_Colon)) {
        TRY(ret.type = parseType(p));
    }

    return ret;
}

static NkAtom parseLabel(ParserState *p) {
    nk_assert(on(p, NklIrToken_AtTag));

    NkString const str = tokenStr(p);
    getToken(p);

    NkAtom const label = nk_s2atom((NkString){str.data + 1, str.size - 1});
    return label;
}

static NkIrInstr parseInstr(ParserState *p) {
    NkIrInstr ret = {0};

    if (on(p, NklIrToken_AtTag)) {
        TRY(NkAtom const label = parseLabel(p));
        ret = nkir_make_label(label);
    }

    else if (accept(p, NklIrToken_nop)) {
        ret = nkir_make_nop();
    }

    else if (accept(p, NklIrToken_ret)) {
        NkIrRef arg = {0};
        if (!on(p, NklToken_Newline)) {
            TRY(arg = parseRef(p, p->proc_ret_t));
        }
        ret = nkir_make_ret(arg);
    }

    else if (accept(p, NklIrToken_jmp)) {
        TRY(NkAtom const label = parseLabel(p));
        ret = nkir_make_jmp(label);
    } else if (accept(p, NklIrToken_jmpz)) {
        TRY(NkIrRef const cond = parseRef(p, NULL));
        EXPECT(NklIrToken_Comma);
        TRY(NkAtom const label = parseLabel(p));
        ret = nkir_make_jmpz(cond, label);
    } else if (accept(p, NklIrToken_jmpnz)) {
        TRY(NkIrRef const cond = parseRef(p, NULL));
        EXPECT(NklIrToken_Comma);
        TRY(NkAtom const label = parseLabel(p));
        ret = nkir_make_jmpnz(cond, label);
    }

    else if (accept(p, NklIrToken_store)) {
        TRY(NkIrRef const src = parseRef(p, NULL));
        EXPECT(NklIrToken_MinusGreater);
        EXPECT(NklIrToken_LBracket);
        TRY(NkIrRef const dst = parseRef(p, NULL));
        ret = nkir_make_store(dst, src);
        EXPECT(NklIrToken_RBracket);
    } else if (accept(p, NklIrToken_load)) {
        EXPECT(NklIrToken_LBracket);
        TRY(NkIrRef const src = parseRef(p, NULL));
        EXPECT(NklIrToken_RBracket);
        EXPECT(NklIrToken_MinusGreater);
        TRY(NkIrRef const dst = parseLocal(p, NULL));
        ret = nkir_make_load(dst, src);
    }

    else if (0) {
    }
#define UNA_IR(NAME)                                  \
    else if (accept(p, NK_CAT(NklIrToken_, NAME))) {  \
        TRY(NkIrRef const arg = parseRef(p, NULL));   \
        EXPECT(NklIrToken_MinusGreater);              \
        TRY(NkIrRef const dst = parseLocal(p, NULL)); \
        ret = NK_CAT(nkir_make_, NAME)(dst, arg);     \
    }
#define BIN_IR(NAME)                                      \
    else if (accept(p, NK_CAT(NklIrToken_, NAME))) {      \
        TRY(NkIrRef const lhs = parseRef(p, NULL));       \
        EXPECT(NklIrToken_Comma);                         \
        TRY(NkIrRef const rhs = parseRef(p, lhs.type));   \
        EXPECT(NklIrToken_MinusGreater);                  \
        TRY(NkIrRef const dst = parseLocal(p, lhs.type)); \
        ret = NK_CAT(nkir_make_, NAME)(dst, lhs, rhs);    \
    }
#include "nkb/ir.inl"

    else if (accept(p, NklIrToken_cmp)) {
        if (0) {
        }
#define CMP_IR(NAME)                                       \
    else if (accept(p, NK_CAT(NklIrToken_, NAME))) {       \
        TRY(NkIrRef const lhs = parseRef(p, NULL));        \
        EXPECT(NklIrToken_Comma);                          \
        TRY(NkIrRef const rhs = parseRef(p, lhs.type));    \
        EXPECT(NklIrToken_MinusGreater);                   \
        TRY(NkIrRef const dst = parseLocal(p, get_i8(p))); \
        ret = NK_CAT(nkir_make_cmp_, NAME)(dst, lhs, rhs); \
    }
#include "nkb/ir.inl"
    }

    else if (on(p, NklToken_Eof)) {
        ERROR("unexpected end of file");
    } else {
        NkString const str = tokenStr(p);
        ERROR("unexpected token `" NKS_FMT "`", NKS_ARG(str));
    }

    EXPECT(NklToken_Newline);
    while (accept(p, NklToken_Newline)) {
    }

    return ret;
}

static Void parseProc(ParserState *p, NkIrVisibility vis) {
    TRY(NklToken const *name_token = expect(p, NklToken_Id));
    NkString const name_str = nkl_getTokenStr(name_token, p->text);
    NkAtom const name = nk_s2atom(name_str);

    EXPECT(NklIrToken_LParen);

    NkIrParamDynArray params = {NKDA_INIT(nk_arena_getAllocator(p->arena))};

    while (!on(p, NklIrToken_RParen) && !on(p, NklToken_Eof)) {
        TRY(NklToken const *arg_name_token = expect(p, NklIrToken_PercentTag));
        NkString const arg_name_str = nkl_getTokenStr(arg_name_token, p->text);
        NkAtom const arg_name = nk_s2atom((NkString){arg_name_str.data + 1, arg_name_str.size - 1});

        EXPECT(NklIrToken_Colon);
        TRY(NkIrType type = parseType(p));

        accept(p, NklIrToken_Comma);

        nkda_append(
            &params,
            ((NkIrParam){
                .name = arg_name,
                .type = type,
            }));
    }

    p->proc_params = (NkIrParamArray){NK_SLICE_INIT(params)};

    EXPECT(NklIrToken_RParen);

    TRY(NkIrType const ret_t = parseType(p));
    p->proc_ret_t = ret_t;

    EXPECT(NklIrToken_LBrace);

    EXPECT(NklToken_Newline);
    while (accept(p, NklToken_Newline)) {
    }

    NkIrInstrDynArray instrs = {NKDA_INIT(nk_arena_getAllocator(p->arena))};

    while (!on(p, NklIrToken_RBrace) && !on(p, NklToken_Eof)) {
        TRY(NkIrInstr instr = parseInstr(p));
        nkda_append(&instrs, instr);
    }

    EXPECT(NklIrToken_RBrace);

    EXPECT(NklToken_Newline);
    while (accept(p, NklToken_Newline)) {
    }

    nkda_append(
        p->ir,
        ((NkIrSymbol){
            .proc =
                {
                    .params = p->proc_params,
                    .ret_type = p->proc_ret_t,
                    .instrs = {NK_SLICE_INIT(instrs)},
                    .file = {0}, // TODO: file
                    .line = 0,   // TODO: line
                    .flags = 0,  // TODO: flags??
                },
            .name = name,
            .vis = vis,
            .flags = 0,
            .kind = NkIrSymbol_Proc,
        }));

    return ret;
}

static Void parseSymbol(ParserState *p) {
    while (accept(p, NklToken_Newline)) {
    }

    if (accept(p, NklIrToken_pub)) {
        if (accept(p, NklIrToken_proc)) {
            TRY(parseProc(p, NkIrVisibility_Default));
        }
    }

    return ret;
}

static Void parse(ParserState *p) {
    nk_assert(p->tokens.size && nk_slice_last(p->tokens).id == NklToken_Eof && "ill-formed token stream");

    while (!on(p, NklToken_Eof)) {
        TRY(parseSymbol(p));
    }

    return ret;
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

        .ir = out_syms,

        .cur_token = p.tokens.data,
    };

    parse(&p);

    nk_arena_free(&p.scratch);

    NK_LOG_STREAM_INF {
        NkStream log = nk_log_getStream();
        nk_printf(log, "IR:\n");
        NkIrModule const syms = {out_syms->data + start_idx, out_syms->size - start_idx};
        nkir_inspectModule(syms, log);
    }

    return !p.error_occurred;
}
