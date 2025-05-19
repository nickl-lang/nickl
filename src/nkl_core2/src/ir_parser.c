#include "nkl/core/ir_parser.h"

#include <stdlib.h>

#include "ir_tokens.h"
#include "nkb/ir.h"
#include "nkb/types.h"
#include "nkl/common/token.h"
#include "nkl/core/lexer.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dl.h"
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

#define ACCEPT(ID) accept(p, (ID))
#define EXPECT(ID) TRY(expect(p, (ID)))

#define ERROR(...)                   \
    do {                             \
        reportError(p, __VA_ARGS__); \
        return ret;                  \
    } while (0)

#define ERROR_EXPECT(FMT, ...)                                                                                \
    do {                                                                                                      \
        if (on(p, NklToken_Eof)) {                                                                            \
            ERROR("unexpected end of file, expected " FMT __VA_OPT__(, ) __VA_ARGS__);                        \
        } else {                                                                                              \
            NkString const _str = escapedCurTokenStr(p);                                                      \
            ERROR("unexpected token `" NKS_FMT "`, expected " FMT, NKS_ARG(_str) __VA_OPT__(, ) __VA_ARGS__); \
        }                                                                                                     \
    } while (0)

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

    NkIrParamDynArray types;
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
        if (!*cached) {                                \
            *cached = (EXPR);                          \
        }                                              \
        return *cached;                                \
    };

#define X(TYPE, VALUE_TYPE) CACHED_TYPE(TYPE, allocNumericType(p, VALUE_TYPE))
NKIR_NUMERIC_ITERATE(X)
#undef X

CACHED_TYPE(void, allocVoidType(p));

#undef CACHED_TYPE

static NkIrType allocStringType(ParserState *p, usize size) {
    NkIrAggregateElemInfo *elem = nk_arena_allocT(p->arena, NkIrAggregateElemInfo);
    *elem = (NkIrAggregateElemInfo){
        .type = get_i8(p),
        .count = size + 1,
        .offset = 0,
    };
    NkIrType_T *type = nk_arena_allocT(p->arena, NkIrType_T);
    *type = (NkIrType_T){
        .aggr = {elem, 1},
        .size = size + 1,
        .align = 1,
        .id = 0,
        .kind = NkIrType_Aggregate,
    };
    return type;
}

static NkIrType get_ptr_t(ParserState *p) {
    return get_i64(p); // TODO: Hardcoded pointer size
}

// TODO: Reuse some code between parsers?

typedef struct {
} Void;
static Void const ret;

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

static NkString escapedCurTokenStr(ParserState *p) {
    NkStringBuilder sb = (NkStringBuilder){NKSB_INIT(nk_arena_getAllocator(&p->scratch))};
    nks_escape(nksb_getStream(&sb), curTokenStr(p));
    return (NkString){NKS_INIT(sb)};
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
    if (!ACCEPT(id)) {
        NkString const str = escapedCurTokenStr(p);
        ERROR(
            "expected `%s` before `" NKS_FMT "`",
            p->token_names ? p->token_names[id] : "??TokenNameUnavailable??",
            NKS_ARG(str));
    }
    return ret;
}

static Void parseNumber(ParserState *p, void *addr, NkString str, NkIrNumericValueType value_type) {
    char const *cstr = nk_tprintf(&p->scratch, NKS_FMT, NKS_ARG(str));

    char *endptr = NULL;

    switch (value_type) {
        case Int8:
            *(i8 *)addr = strtol(cstr, &endptr, 10);
            break;
        case Uint8:
            *(u8 *)addr = strtoul(cstr, &endptr, 10);
            break;
        case Int16:
            *(i16 *)addr = strtol(cstr, &endptr, 10);
            break;
        case Uint16:
            *(u16 *)addr = strtoul(cstr, &endptr, 10);
            break;
        case Int32:
            *(i32 *)addr = strtol(cstr, &endptr, 10);
            break;
        case Uint32:
            *(u32 *)addr = strtoul(cstr, &endptr, 10);
            break;
        case Int64:
            *(i64 *)addr = strtoll(cstr, &endptr, 10);
            break;
        case Uint64:
            *(u64 *)addr = strtoull(cstr, &endptr, 10);
            break;
        case Float32:
            *(f32 *)addr = strtof(cstr, &endptr);
            break;
        case Float64:
            *(f64 *)addr = strtod(cstr, &endptr);
            break;
    }

    if (endptr != cstr + str.size) {
        ERROR("failed to parse numeric constant");
    }

    return ret;
}

static u32 parseIdx(ParserState *p) {
    u32 ret = 0;

    if (!on(p, NklToken_Int)) {
        ERROR("integer constant expected");
    }

    NkString const token_str = getToken(p);
    TRY(parseNumber(p, &ret, token_str, Uint32));

    return ret;
}

static NkIrType parseType(ParserState *p) {
    NkIrType ret = NULL;

#define X(TYPE, VALUE_TYPE)                  \
    if (ACCEPT(NK_CAT(NklIrToken_, TYPE))) { \
        return NK_CAT(get_, TYPE)(p);        \
    }
    NKIR_NUMERIC_ITERATE(X)
#undef X

    if (ACCEPT(NklIrToken_void)) {
        return get_void(p);
    }

    if (ACCEPT(NklIrToken_LBrace)) {
        NkIrAggregateElemInfoDynArray elems = {NKDA_INIT(nk_arena_getAllocator(p->arena))};
        NkIrTypeKind kind = NkIrType_Aggregate;

        u32 offset = 0;
        u8 align = 1;
        while (!on(p, NklIrToken_RBrace) && !on(p, NklToken_Eof)) {
            u32 count = 1;

            if (ACCEPT(NklIrToken_LBracket)) {
                TRY(count = parseIdx(p));
                EXPECT(NklIrToken_RBracket);
            }

            TRY(NkIrType const type = parseType(p));

            align = nk_maxu(align, type->align);
            offset = nk_roundUpSafe(offset, type->align);
            nkda_append(
                &elems,
                ((NkIrAggregateElemInfo){
                    .type = type,
                    .count = count,
                    .offset = offset,
                }));
            offset += type->size * count;

            if (!on(p, NklIrToken_RBrace)) {
                if (kind) {
                    if (kind == NkIrType_Aggregate) {
                        EXPECT(NklIrToken_Comma);
                    } else {
                        EXPECT(NklIrToken_Pipe);
                    }
                } else {
                    if (ACCEPT(NklIrToken_Comma)) {
                        kind = NkIrType_Aggregate;
                    } else if (ACCEPT(NklIrToken_Pipe)) {
                        kind = NkIrType_Union;
                    } else {
                        ERROR_EXPECT("`,` or `|`"); // TODO: Actually take into account union type
                    }
                }
            }
        }
        u64 const size = nk_roundUpSafe(offset, align);

        EXPECT(NklIrToken_RBrace);

        NkIrType_T *type = nk_arena_allocT(p->arena, NkIrType_T);
        *type = (NkIrType_T){
            .aggr = {NK_SLICE_INIT(elems)},
            .size = size,
            .align = align,
            .id = 0,
            .kind = NkIrType_Aggregate,
        };

        return type;
    } else if (on(p, NklToken_Id)) {
        TRY(NklToken const *name_token = expect(p, NklToken_Id));
        NkString const name_token_str = tokenStr(p, name_token);
        NkAtom const name = nk_s2atom(name_token_str);

        for (usize i = 0; i < p->types.size; i++) {
            NkIrParam const *rec = &p->types.data[i];
            if (rec->name == name) {
                return rec->type;
            }
        }

        ERROR("undefined type alias `" NKS_FMT "`", NKS_ARG(name_token_str));
    }

    ERROR_EXPECT("a type");
}

static NkString parseString(ParserState *p) {
    NkString ret = {0};

    if (!on(p, NklToken_String) && !on(p, NklToken_EscapedString)) {
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

static Void parseNumericConst(ParserState *p, void *addr, NkIrType type) {
    nk_assert(type->kind == NkIrType_Numeric);

    if (on(p, NklToken_Int)) {
        NkString const token_str = getToken(p);
        TRY(parseNumber(p, addr, token_str, type->num));
    } else if (on(p, NklToken_Float)) {
        if (!NKIR_NUMERIC_IS_FLT(type->num)) {
            ERROR("incorrect value for type `TODO:nkir_inspectType`");
        }

        NkString const token_str = getToken(p);
        TRY(parseNumber(p, addr, token_str, type->num));
    }

    else {
        // TODO: Improve error message accuracy
        ERROR_EXPECT("a numeric constant");
    }

    return ret;
}

static NkIrRelocArray parseConst(ParserState *p, void *addr, NkIrType type) {
    NkIrRelocArray ret = {0};

    switch (type->kind) {
        case NkIrType_None:
            break;

        case NkIrType_Union: {
            ERROR("TODO: parse union const");
            break;
        }

        case NkIrType_Aggregate: {
            EXPECT(NklIrToken_LBrace);

            usize offset = 0;
            for (usize elemi = 0; elemi < type->aggr.size; elemi++) {
                NkIrAggregateElemInfo const *elem = &type->aggr.data[elemi];
                offset = elem->offset;
                if (elem->count > 1) {
                    EXPECT(NklIrToken_LBracket);
                }
                for (usize i = 0; i < elem->count; i++) {
                    // TODO: Generate & merge relocs
                    TRY(parseConst(p, (u8 *)addr + offset, elem->type));
                    offset += elem->type->size;
                    if (i == elem->count - 1) {
                        ACCEPT(NklIrToken_Comma);
                    } else {
                        EXPECT(NklIrToken_Comma);
                    }
                }
                if (elem->count > 1) {
                    EXPECT(NklIrToken_RBracket);
                }
            }

            EXPECT(NklIrToken_RBrace);
            break;
        }

        case NkIrType_Numeric: {
            TRY(parseNumericConst(p, addr, type));
            break;
        }
    }

    return ret;
}

static NkIrRef parseLocal(ParserState *p, NkIrType type_opt, bool to_write) {
    NkIrRef ret = {0};

    NkIrType type = type_opt;

    NkString const token_str = getToken(p);
    NkAtom const name = nk_s2atom((NkString){token_str.data + 1, token_str.size - 1});

    bool is_param = false;

    for (usize i = 0; i < p->proc_params.size; i++) {
        NkIrParam const *param = &p->proc_params.data[i];
        if (name == param->name) {
            if (!type) {
                if (param->type->kind == NkIrType_Numeric) {
                    type = param->type;
                } else {
                    type = get_ptr_t(p);
                }
            }
            is_param = true;
            break;
        }
    }

    if (!type) {
        p->cur_token--;
        ERROR("type must be specified");
    }

    if (is_param && to_write) {
        p->cur_token--;
        ERROR("params are read-only");
    }

    return is_param ? nkir_makeRefParam(name, type) : nkir_makeRefLocal(name, type);
}

static NkIrRef parseDst(ParserState *p, NkIrType type_opt, bool allow_null) {
    NkIrRef ret = {0};

    NkIrType type = NULL;
    if (ACCEPT(NklIrToken_Colon)) {
        TRY(type = parseType(p));
    } else {
        type = type_opt;
    }

    if (on(p, NklToken_Newline) && allow_null) {
        if (!type) {
            ERROR("type must be specified");
        }
        return nkir_makeRefNull(type);
    } else {
        if (!on(p, NklIrToken_PercentTag)) {
            ERROR_EXPECT("a local");
        }

        return parseLocal(p, type, true);
    }
}

static NkIrRef parseRef(ParserState *p, NkIrType type_opt) {
    NkIrRef ret = {0};

    if (ACCEPT(NklIrToken_Ellipsis)) {
        return nkir_makeVariadicMarker();
    }

    NkIrType type = NULL;
    if (ACCEPT(NklIrToken_Colon)) {
        TRY(type = parseType(p));
    } else {
        type = type_opt;
    }

    if (!type) {
        if (on(p, NklToken_Int)) {
            type = get_i64(p);
        } else if (on(p, NklToken_Float)) {
            type = get_f64(p);
        }
    }

    if (on(p, NklIrToken_PercentTag)) {
        return parseLocal(p, type, false);
    }

    else if (on(p, NklIrToken_DollarTag)) {
        NkString const token_str = getToken(p);
        NkAtom const sym = nk_s2atom((NkString){token_str.data + 1, token_str.size - 1});

        return nkir_makeRefGlobal(sym, get_ptr_t(p));
    }

    else if (type) {
        if (type->kind == NkIrType_Numeric) {
            NkIrImm imm;
            TRY(parseConst(p, &imm, type));
            return nkir_makeRefImm(imm, type);
        } else {
            void *addr = nk_arena_allocAligned(p->arena, type->size, type->align);
            TRY(NkIrRelocArray relocs = parseConst(p, addr, type));

            // TODO: A little boilerplate
            NkAtom const sym = nk_atom_unique((NkString){0});
            nkda_append(
                p->ir,
                ((NkIrSymbol){
                    .data =
                        {
                            .type = type,
                            .relocs = relocs,
                            .addr = addr,
                            .flags = NkIrData_ReadOnly,
                        },
                    .name = sym,
                    .vis = NkIrVisibility_Local,
                    .flags = 0,
                    .kind = NkIrSymbol_Data,
                }));
            return nkir_makeRefGlobal(sym, get_ptr_t(p));
        }
    }

    else if (on(p, NklToken_String) || on(p, NklToken_EscapedString)) {
        TRY(NkString const str = parseString(p));

        NkIrType type = allocStringType(p, str.size);

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
        return nkir_makeRefGlobal(sym, get_ptr_t(p));
    }

    else if (on(p, NklIrToken_LBrace)) {
        ERROR("TODO: parse untyped aggregate");
    }

    ERROR_EXPECT("a reference");
}

static NkIrRefArray parseRefArray(ParserState *p) {
    NkIrRefArray ret = {0};

    NkIrRefDynArray refs = {NKDA_INIT(nk_arena_getAllocator(p->arena))};

    EXPECT(NklIrToken_LParen);

    while (!on(p, NklIrToken_RParen) && !on(p, NklToken_Eof)) {
        TRY(NkIrRef const ref = parseRef(p, NULL));
        nkda_append(&refs, ref);
        ACCEPT(NklIrToken_Comma);
    }

    EXPECT(NklIrToken_RParen);

    ret = (NkIrRefArray){NK_SLICE_INIT(refs)};

    return ret;
}

static NkAtom parseLabel(ParserState *p) {
    nk_assert(on(p, NklIrToken_AtTag));

    NkString const token_str = getToken(p);
    NkAtom const label = nk_s2atom((NkString){token_str.data + 1, token_str.size - 1});
    return label;
}

static NkIrInstr parseInstr(ParserState *p) {
    NkIrInstr ret = {0};

    if (ACCEPT(NklToken_Int)) {
        EXPECT(NklIrToken_Pipe);
    }

    if (on(p, NklIrToken_AtTag)) {
        TRY(NkAtom const label = parseLabel(p));
        ret = nkir_make_label(label);
    }

    else if (ACCEPT(NklIrToken_nop)) {
        ret = nkir_make_nop();
    }

    else if (ACCEPT(NklIrToken_ret)) {
        NkIrRef arg = {0};
        if (!on(p, NklToken_Newline)) {
            TRY(arg = parseRef(p, p->proc_ret_t));
        }
        ret = nkir_make_ret(arg);
    }

    else if (ACCEPT(NklIrToken_jmp)) {
        TRY(NkAtom const label = parseLabel(p));
        ret = nkir_make_jmp(label);
    } else if (ACCEPT(NklIrToken_jmpz)) {
        TRY(NkIrRef const cond = parseRef(p, NULL));
        EXPECT(NklIrToken_Comma);
        TRY(NkAtom const label = parseLabel(p));
        ret = nkir_make_jmpz(cond, label);
    } else if (ACCEPT(NklIrToken_jmpnz)) {
        TRY(NkIrRef const cond = parseRef(p, NULL));
        EXPECT(NklIrToken_Comma);
        TRY(NkAtom const label = parseLabel(p));
        ret = nkir_make_jmpnz(cond, label);
    }

    else if (ACCEPT(NklIrToken_call)) {
        TRY(NkIrRef const proc = parseRef(p, get_ptr_t(p)));
        EXPECT(NklIrToken_Comma);
        TRY(NkIrRefArray const args = parseRefArray(p));
        NkIrRef dst = nkir_makeRefNull(get_void(p));
        if (ACCEPT(NklIrToken_MinusGreater)) {
            TRY(dst = parseDst(p, NULL, true));
        }
        ret = nkir_make_call(dst, proc, args);
    }

    else if (ACCEPT(NklIrToken_store)) {
        TRY(NkIrRef const src = parseRef(p, NULL));
        EXPECT(NklIrToken_MinusGreater);
        EXPECT(NklIrToken_LBracket);
        TRY(NkIrRef const dst = parseRef(p, get_ptr_t(p)));
        ret = nkir_make_store(dst, src);
        EXPECT(NklIrToken_RBracket);
    } else if (ACCEPT(NklIrToken_load)) {
        EXPECT(NklIrToken_LBracket);
        TRY(NkIrRef const src = parseRef(p, get_ptr_t(p)));
        EXPECT(NklIrToken_RBracket);
        EXPECT(NklIrToken_MinusGreater);
        TRY(NkIrRef const dst = parseDst(p, NULL, false));
        ret = nkir_make_load(dst, src);
    }

    else if (ACCEPT(NklIrToken_alloc)) {
        EXPECT(NklIrToken_Colon);
        TRY(NkIrType const type = parseType(p));
        EXPECT(NklIrToken_MinusGreater);
        TRY(NkIrRef const dst = parseDst(p, get_ptr_t(p), false));
        ret = nkir_make_alloc(dst, type);
    }

    else if (0) {
    }
    // TODO: Check and derive types in UNA and BIN IR
#define UNA_IR(NAME)                                       \
    else if (ACCEPT(NK_CAT(NklIrToken_, NAME))) {          \
        TRY(NkIrRef const arg = parseRef(p, NULL));        \
        EXPECT(NklIrToken_MinusGreater);                   \
        TRY(NkIrRef const dst = parseDst(p, NULL, false)); \
        ret = NK_CAT(nkir_make_, NAME)(dst, arg);          \
    }
#define BIN_IR(NAME)                                           \
    else if (ACCEPT(NK_CAT(NklIrToken_, NAME))) {              \
        TRY(NkIrRef const lhs = parseRef(p, NULL));            \
        EXPECT(NklIrToken_Comma);                              \
        TRY(NkIrRef const rhs = parseRef(p, lhs.type));        \
        EXPECT(NklIrToken_MinusGreater);                       \
        TRY(NkIrRef const dst = parseDst(p, lhs.type, false)); \
        ret = NK_CAT(nkir_make_, NAME)(dst, lhs, rhs);         \
    }
#include "nkb/ir.inl"

    else if (ACCEPT(NklIrToken_cmp)) {
        if (0) {
        }
#define CMP_IR(NAME)                                            \
    else if (ACCEPT(NK_CAT(NklIrToken_, NAME))) {               \
        TRY(NkIrRef const lhs = parseRef(p, NULL));             \
        EXPECT(NklIrToken_Comma);                               \
        TRY(NkIrRef const rhs = parseRef(p, lhs.type));         \
        EXPECT(NklIrToken_MinusGreater);                        \
        TRY(NkIrRef const dst = parseDst(p, get_i8(p), false)); \
        ret = NK_CAT(nkir_make_cmp_, NAME)(dst, lhs, rhs);      \
    }
#include "nkb/ir.inl"
    }

    else if (ACCEPT(NklIrToken_file)) {
        TRY(NkString const str = parseString(p));
        ret = nkir_make_file(str);
    } else if (ACCEPT(NklIrToken_comment)) {
        TRY(NkString const str = parseString(p));
        ret = nkir_make_comment(str);
    }

    else if (ACCEPT(NklIrToken_line)) {
        TRY(u32 const idx = parseIdx(p));
        ret = nkir_make_line(idx);
    }

    else {
        ERROR_EXPECT("an instruction");
    }

    EXPECT(NklToken_Newline);
    while (ACCEPT(NklToken_Newline)) {
    }

    return ret;
}

static Void parseProc(ParserState *p, NkIrVisibility vis) {
    TRY(NklToken const *name_token = expect(p, NklIrToken_DollarTag));
    NkString const name_token_str = tokenStr(p, name_token);
    NkAtom const name = nk_s2atom((NkString){name_token_str.data + 1, name_token_str.size - 1});

    EXPECT(NklIrToken_LParen);

    NkIrParamDynArray params = {NKDA_INIT(nk_arena_getAllocator(p->arena))};

    while (!on(p, NklIrToken_RParen) && !on(p, NklToken_Eof)) {
        TRY(NklToken const *arg_name_token = expect(p, NklIrToken_PercentTag));
        NkString const arg_name_token_str = tokenStr(p, arg_name_token);
        NkAtom const arg_name = nk_s2atom((NkString){arg_name_token_str.data + 1, arg_name_token_str.size - 1});

        EXPECT(NklIrToken_Colon);
        TRY(NkIrType const type = parseType(p));

        ACCEPT(NklIrToken_Comma);

        nkda_append(
            &params,
            ((NkIrParam){
                .name = arg_name,
                .type = type,
            }));
    }

    p->proc_params = (NkIrParamArray){NK_SLICE_INIT(params)};

    EXPECT(NklIrToken_RParen);

    EXPECT(NklIrToken_Colon);
    TRY(NkIrType const ret_t = parseType(p));
    p->proc_ret_t = ret_t;

    EXPECT(NklIrToken_LBrace);

    EXPECT(NklToken_Newline);
    while (ACCEPT(NklToken_Newline)) {
    }

    NkIrInstrDynArray instrs = {NKDA_INIT(nk_arena_getAllocator(p->arena))};

    while (!on(p, NklIrToken_RBrace) && !on(p, NklToken_Eof)) {
        TRY(NkIrInstr const instr = parseInstr(p));
        nkda_append(&instrs, instr);
    }

    EXPECT(NklIrToken_RBrace);

    EXPECT(NklToken_Newline);
    while (ACCEPT(NklToken_Newline)) {
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
    TRY(NklToken const *name_token = expect(p, NklIrToken_DollarTag));
    NkString const token_str = tokenStr(p, name_token);
    NkAtom const name = nk_s2atom((NkString){token_str.data + 1, token_str.size - 1});

    NkIrType type = NULL;
    if (ACCEPT(NklIrToken_Colon)) {
        TRY(type = parseType(p));
    }

    if (!type) {
        if (on(p, NklToken_Int)) {
            type = get_i64(p);
        } else if (on(p, NklToken_Float)) {
            type = get_f64(p);
        }
    }

    void *addr = NULL;
    NkIrRelocArray relocs = {0};
    if (!on(p, NklToken_Newline)) {
        if (type) {
            addr = nk_arena_allocAligned(p->arena, type->size, type->align);
            TRY(relocs = parseConst(p, addr, type));
        }

        else if (on(p, NklToken_String) || on(p, NklToken_EscapedString)) {
            TRY(NkString const str = parseString(p));

            type = allocStringType(p, str.size);
            addr = (void *)str.data;
        }

        else if (on(p, NklIrToken_LBrace)) {
            ERROR("TODO: parse untyped aggregate");
        }

        else {
            ERROR_EXPECT("a constant");
        }
    }

    if (!addr && flags & NkIrData_ReadOnly) {
        ERROR("constant must have a value");
    }

    if (!type) {
        ERROR_EXPECT("a type or a constant");
    }

    EXPECT(NklToken_Newline);
    while (ACCEPT(NklToken_Newline)) {
    }

    nkda_append(
        p->ir,
        ((NkIrSymbol){
            .data =
                {
                    .type = type,
                    .relocs = relocs,
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

static Void parseExtern(ParserState *p) {
    TRY(NkString lib_str = parseString(p));
    if (nks_equalCStr(lib_str, "c")) { // TODO: Hardcoded libc, implement lib aliases
        lib_str = nk_cs2s("libc.so.6");
    } else if (nks_equalCStr(lib_str, "m")) {
        lib_str = nk_cs2s("libm.so.6");
    }
    char const *lib_nt = nk_tprintf(&p->scratch, NKS_FMT, NKS_ARG(lib_str));

    TRY(NklToken const *sym_token = expect(p, NklIrToken_DollarTag));
    NkString const token_str = tokenStr(p, sym_token);
    NkString const sym_str = {token_str.data + 1, token_str.size - 1};
    char const *sym_nt = nk_tprintf(&p->scratch, NKS_FMT, NKS_ARG(sym_str));

    NkHandle dl = nkdl_loadLibrary(lib_nt);
    if (nk_handleIsNull(dl)) {
        ERROR("failed to load library `" NKS_FMT "`: %s", NKS_ARG(lib_str), nkdl_getLastErrorString());
    }

    void *sym = nkdl_resolveSymbol(dl, sym_nt);
    if (!sym) {
        ERROR("failed to load symbol `" NKS_FMT "`: %s", NKS_ARG(sym_str), nkdl_getLastErrorString());
    }

    // TODO: Store extern lib for reproducibility purposes

    nkda_append(
        p->ir,
        ((NkIrSymbol){
            .extrn =
                {
                    .addr = sym,
                },
            .name = nk_s2atom(sym_str),
            .vis = 0,
            .flags = 0,
            .kind = NkIrSymbol_Extern,
        }));

    return ret;
}

static Void parseSymbol(ParserState *p) {
    while (ACCEPT(NklToken_Newline)) {
    }

    NkIrVisibility vis = NkIrVisibility_Hidden;

    if (ACCEPT(NklIrToken_extern)) {
        return parseExtern(p);
    }

    if (ACCEPT(NklIrToken_type)) {
        TRY(NklToken const *name_token = expect(p, NklToken_Id));
        NkString const name_token_str = tokenStr(p, name_token);
        NkAtom const name = nk_s2atom(name_token_str);

        EXPECT(NklIrToken_Colon);
        TRY(NkIrType const type = parseType(p));

        nkda_append(
            &p->types,
            ((NkIrParam){
                .name = name,
                .type = type,
            }));

        EXPECT(NklToken_Newline);
        while (ACCEPT(NklToken_Newline)) {
        }
    }

    if (ACCEPT(NklIrToken_pub)) {
        vis = NkIrVisibility_Default;
    } else if (ACCEPT(NklIrToken_local)) {
        vis = NkIrVisibility_Local;
    }

    if (ACCEPT(NklIrToken_proc)) {
        return parseProc(p, vis);
    } else if (ACCEPT(NklIrToken_const)) {
        return parseData(p, vis, NkIrData_ReadOnly);
    } else if (ACCEPT(NklIrToken_data)) {
        return parseData(p, vis, 0);
    }

    ERROR_EXPECT("a symbol");
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

        .types = {NKDA_INIT(nk_arena_getAllocator(p.arena))},
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
