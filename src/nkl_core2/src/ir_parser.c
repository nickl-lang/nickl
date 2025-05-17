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
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(ir_parser);

#define TRY(STMT)               \
    STMT;                       \
    while (p->error_occurred) { \
        return ret;             \
    }

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
    }
    if (p->err_token) {
        *p->err_token = *p->cur_token;
    }
    p->error_occurred = true;
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

static NkString tokenStr(ParserState *p, NklToken const *token) {
    return nkl_getTokenStr(token, p->text);
}

static NkString curTokenStr(ParserState *p) {
    return tokenStr(p, p->cur_token);
}

static void getTokenImpl(ParserState *p) {
    nk_assert(!on(p, NklToken_Eof));
    p->cur_token++;

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_printf(log, "next token: \"");
        nks_escape(log, curTokenStr(p));
        nk_printf(log, "\":%u", p->cur_token->id);
    }
}

static NkString getToken(ParserState *p) {
    NkString ret = curTokenStr(p);
    getTokenImpl(p);
    return ret;
}

static bool accept(ParserState *p, u32 id) {
    if (on(p, id)) {
        NK_LOG_STREAM_DBG {
            NkStream log = nk_log_getStream();
            nk_printf(log, "accept \"");
            nks_escape(log, curTokenStr(p));
            nk_printf(log, "\":%u", p->cur_token->id);
        }

        getTokenImpl(p);
        return true;
    }
    return false;
}

static NklToken const *expect(ParserState *p, u32 id) {
    NklToken const *ret = p->cur_token;
    if (!accept(p, id)) {
        NkString const str = curTokenStr(p);
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
        ERROR("type expected");
    }

    return ret;
}

static NkIrImm parseImm(ParserState *p, NkString str, NkIrNumericValueType value_type) {
    NkIrImm ret = {0};

    char const *cstr = nk_tprintf(&p->scratch, NKS_FMT, NKS_ARG(str));

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

static NkIrRef parseLocal(ParserState *p, NkIrType type_opt, bool to_write) {
    NkIrRef ret = {0};

    if (!on(p, NklIrToken_PercentTag)) {
        ERROR("local expected");
    }

    NkString const str = getToken(p);
    NkAtom const local = nk_s2atom((NkString){str.data + 1, str.size - 1});

    NkIrType type = type_opt;
    bool is_param = false;

    for (usize i = 0; i < p->proc_params.size; i++) {
        NkIrParam const *param = &p->proc_params.data[i];
        if (local == param->name) {
            if (!type) {
                type = param->type;
            }
            is_param = true;
            break;
        }
    }

    if (is_param && to_write) {
        p->cur_token--;
        ERROR("cannot write to param");
    }

    if (!type) {
        EXPECT(NklIrToken_Colon);
        TRY(type = parseType(p));
    } else if (accept(p, NklIrToken_Colon)) {
        TRY(type = parseType(p));
    }

    return is_param ? nkir_makeRefParam(local, type) : nkir_makeRefLocal(local, type);
}

static NkString parseString(ParserState *p) {
    NkString ret = {0};

    if (!(on(p, NklToken_String) || on(p, NklToken_EscapedString))) {
        ERROR("string expected");
    }

    u32 const token_id = p->cur_token->id;

    NkString const token_str = getToken(p);
    NkString const str = (NkString){token_str.data + 1, token_str.size - 2};

    if (token_id == NklToken_String) {
        char const *cstr = nk_tprintf(p->arena, NKS_FMT, NKS_ARG(str));
        return (NkString){cstr, str.size};
    } else {
        NkStringBuilder sb = {NKSB_INIT(nk_arena_getAllocator(p->arena))};
        nks_unescape(nksb_getStream(&sb), str);
        nksb_appendNull(&sb);

        return (NkString){sb.data, sb.size - 1};
    }
}

static u32 parseIdx(ParserState *p) {
    u32 ret = 0;

    if (!on(p, NklToken_Int)) {
        ERROR("integer constant expected");
    }

    NkString const str = getToken(p);
    TRY(NkIrImm imm = parseImm(p, str, Uint32));

    return imm.u32;
}

static NkIrRef parseRef(ParserState *p, NkIrType type_opt) {
    NkIrRef ret = {0};

    if (accept(p, NklIrToken_Ellipsis)) {
        return nkir_makeVariadicMarker();
    }

    if (on(p, NklToken_Int)) {
        NkString const str = getToken(p);
        NkIrType type = NULL;
        if (accept(p, NklIrToken_Colon)) {
            TRY(type = parseType(p));
        } else if (type_opt && type_opt->kind == NkIrType_Numeric) {
            type = type_opt;
        } else {
            type = get_i64(p);
        }
        TRY(NkIrImm imm = parseImm(p, str, type->num));
        return nkir_makeRefImm(imm, type);
    } else if (on(p, NklToken_Float)) {
        NkString const str = getToken(p);
        NkIrType type = NULL;
        if (accept(p, NklIrToken_Colon)) {
            TRY(type = parseType(p));
        } else if (type_opt && type_opt->kind == NkIrType_Numeric) {
            type = type_opt;
        } else {
            type = get_f64(p);
        }
        if (!NKIR_NUMERIC_IS_FLT(type->num)) {
            p->cur_token--;
            ERROR("floating point type expected");
        }
        TRY(NkIrImm imm = parseImm(p, str, type->num));
        return nkir_makeRefImm(imm, type);
    }

    else if (on(p, NklToken_String) || on(p, NklToken_EscapedString)) {
        TRY(NkString const str = parseString(p));

        NkIrAggregateElemInfo *elem = nk_arena_allocT(p->arena, NkIrAggregateElemInfo);
        *elem = (NkIrAggregateElemInfo){
            .type = get_i8(p),
            .count = str.size + 1,
            .offset = 0,
        };
        NkIrType_T *type = nk_arena_allocT(p->arena, NkIrType_T);
        *type = (NkIrType_T){
            .aggr = {elem, 1},
            .size = str.size + 1,
            .align = 1,
            .id = 0,
            .kind = NkIrType_Aggregate,
        };

        NkAtom const sym = nk_atom_unique((NkString){0});

        nkda_append(
            p->ir,
            ((NkIrSymbol){
                .data =
                    {
                        .type = type,
                        .relocs = {0},
                        .addr = (void *)str.data,
                        .flags = NkIrData_ReadOnly,
                    },
                .name = sym,
                .vis = NkIrVisibility_Local,
                .flags = 0,
                .kind = NkIrSymbol_Data,
            }));

        return nkir_makeRefGlobal(sym, type);
    }

    else if (on(p, NklIrToken_PercentTag)) {
        TRY(NkIrRef const ref = parseLocal(p, type_opt, false));
        return ref;
    }

    else if (on(p, NklToken_Id)) {
        NkString const str = getToken(p);
        NkAtom const id = nk_s2atom(str);

        EXPECT(NklIrToken_Colon);
        TRY(NkIrType const type = parseType(p));

        return nkir_makeRefGlobal(id, type);
    }

    else if (on(p, NklToken_Eof)) {
        ERROR("unexpected end of file, expected a reference");
    } else {
        NkString const str = curTokenStr(p);
        ERROR("unexpected token `" NKS_FMT "`, expected a reference", NKS_ARG(str));
    }

    return ret;
}

static NkIrRefArray parseRefArray(ParserState *p) {
    NkIrRefArray ret = {0};

    NkIrRefDynArray refs = {NKDA_INIT(nk_arena_getAllocator(p->arena))};

    EXPECT(NklIrToken_LParen);

    while (!on(p, NklIrToken_RParen) && !on(p, NklToken_Eof)) {
        TRY(NkIrRef const ref = parseRef(p, NULL));
        nkda_append(&refs, ref);
        accept(p, NklIrToken_Comma);
    }

    EXPECT(NklIrToken_RParen);

    ret = (NkIrRefArray){NK_SLICE_INIT(refs)};

    return ret;
}

static NkAtom parseLabel(ParserState *p) {
    nk_assert(on(p, NklIrToken_AtTag));

    NkString const str = getToken(p);
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

    else if (accept(p, NklIrToken_call)) {
        TRY(NkIrRef const proc = parseRef(p, NULL)); // TODO: Infer pointer types
        EXPECT(NklIrToken_Comma);
        TRY(NkIrRefArray const args = parseRefArray(p));
        EXPECT(NklIrToken_MinusGreater);
        TRY(NkIrRef const dst = parseLocal(p, NULL, true));
        ret = nkir_make_call(dst, proc, args);
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
        TRY(NkIrRef const dst = parseLocal(p, NULL, true));
        ret = nkir_make_load(dst, src);
    }

    else if (accept(p, NklIrToken_alloc)) {
        EXPECT(NklIrToken_Colon);
        TRY(NkIrType const type = parseType(p));
        EXPECT(NklIrToken_MinusGreater);
        TRY(NkIrRef const dst = parseLocal(p, NULL, true));
        ret = nkir_make_alloc(dst, type);
    }

    else if (0) {
    }
    // TODO: Check and derive types in UNA and BIN IR
#define UNA_IR(NAME)                                        \
    else if (accept(p, NK_CAT(NklIrToken_, NAME))) {        \
        TRY(NkIrRef const arg = parseRef(p, NULL));         \
        EXPECT(NklIrToken_MinusGreater);                    \
        TRY(NkIrRef const dst = parseLocal(p, NULL, true)); \
        ret = NK_CAT(nkir_make_, NAME)(dst, arg);           \
    }
#define BIN_IR(NAME)                                            \
    else if (accept(p, NK_CAT(NklIrToken_, NAME))) {            \
        TRY(NkIrRef const lhs = parseRef(p, NULL));             \
        EXPECT(NklIrToken_Comma);                               \
        TRY(NkIrRef const rhs = parseRef(p, lhs.type));         \
        EXPECT(NklIrToken_MinusGreater);                        \
        TRY(NkIrRef const dst = parseLocal(p, lhs.type, true)); \
        ret = NK_CAT(nkir_make_, NAME)(dst, lhs, rhs);          \
    }
#include "nkb/ir.inl"

    else if (accept(p, NklIrToken_cmp)) {
        if (0) {
        }
#define CMP_IR(NAME)                                             \
    else if (accept(p, NK_CAT(NklIrToken_, NAME))) {             \
        TRY(NkIrRef const lhs = parseRef(p, NULL));              \
        EXPECT(NklIrToken_Comma);                                \
        TRY(NkIrRef const rhs = parseRef(p, lhs.type));          \
        EXPECT(NklIrToken_MinusGreater);                         \
        TRY(NkIrRef const dst = parseLocal(p, get_i8(p), true)); \
        ret = NK_CAT(nkir_make_cmp_, NAME)(dst, lhs, rhs);       \
    }
#include "nkb/ir.inl"
    }

    else if (accept(p, NklIrToken_file)) {
        TRY(NkString const str = parseString(p));
        ret = nkir_make_file(str);
    } else if (accept(p, NklIrToken_comment)) {
        TRY(NkString const str = parseString(p));
        ret = nkir_make_comment(str);
    }

    else if (accept(p, NklIrToken_line)) {
        TRY(u32 const idx = parseIdx(p));
        ret = nkir_make_line(idx);
    }

    else if (on(p, NklToken_Eof)) {
        ERROR("unexpected end of file, expected an instruction");
    } else {
        NkString const str = curTokenStr(p);
        ERROR("unexpected token `" NKS_FMT "`, expected an instruction", NKS_ARG(str));
    }

    EXPECT(NklToken_Newline);
    while (accept(p, NklToken_Newline)) {
    }

    return ret;
}

static Void parseProc(ParserState *p, NkIrVisibility vis) {
    TRY(NklToken const *name_token = expect(p, NklToken_Id));
    NkString const name_str = tokenStr(p, name_token);
    NkAtom const name = nk_s2atom(name_str);

    EXPECT(NklIrToken_LParen);

    NkIrParamDynArray params = {NKDA_INIT(nk_arena_getAllocator(p->arena))};

    while (!on(p, NklIrToken_RParen) && !on(p, NklToken_Eof)) {
        TRY(NklToken const *arg_name_token = expect(p, NklIrToken_PercentTag));
        NkString const arg_name_str = tokenStr(p, arg_name_token);
        NkAtom const arg_name = nk_s2atom((NkString){arg_name_str.data + 1, arg_name_str.size - 1});

        EXPECT(NklIrToken_Colon);
        TRY(NkIrType const type = parseType(p));

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
        TRY(NkIrInstr const instr = parseInstr(p));
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

static Void parseData(ParserState *p, NkIrVisibility vis, NkIrDataFlags flags) {
    TRY(NklToken const *name_token = expect(p, NklToken_Id));
    NkString const name_str = tokenStr(p, name_token);
    NkAtom const name = nk_s2atom(name_str);

    NkIrType type = NULL;
    if (accept(p, NklIrToken_Colon)) {
        TRY(type = parseType(p));
    }

    if (flags & NkIrData_ReadOnly) {
        EXPECT(NklIrToken_Equal);
        p->cur_token--; // TODO: A hack. Maybe add assertOn?
    }

    void *addr = NULL;
    if (accept(p, NklIrToken_Equal)) {
        // TODO: Total hack
        u32 *val = nk_arena_allocT(p->arena, u32);
        TRY(*val = parseIdx(p));
        addr = val;
        if (!type) {
            type = get_u32(p);
        }
    }

    EXPECT(NklToken_Newline);
    while (accept(p, NklToken_Newline)) {
    }

    nkda_append(
        p->ir,
        ((NkIrSymbol){
            .data =
                {
                    .type = type,
                    .relocs = {0},
                    .addr = addr,
                    .flags = flags,
                },
            .name = name,
            .vis = vis,
            .flags = 0,
            .kind = NkIrSymbol_Data,
        }));

    return ret;
}

static Void parseSymbol(ParserState *p) {
    while (accept(p, NklToken_Newline)) {
    }

    NkIrVisibility vis = NkIrVisibility_Hidden;

    if (accept(p, NklIrToken_pub)) {
        vis = NkIrVisibility_Default;
    } else if (accept(p, NklIrToken_local)) {
        vis = NkIrVisibility_Local;
    }

    if (accept(p, NklIrToken_proc)) {
        TRY(parseProc(p, vis));
    } else if (accept(p, NklIrToken_const)) {
        TRY(parseData(p, vis, NkIrData_ReadOnly));
    } else if (accept(p, NklIrToken_data)) {
        TRY(parseData(p, vis, 0));
    }

    else if (on(p, NklToken_Eof)) {
        ERROR("unexpected end of file, expected a symbol");
    } else {
        NkString const str = curTokenStr(p);
        ERROR("unexpected token `" NKS_FMT "`, expected a symbol", NKS_ARG(str));
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
