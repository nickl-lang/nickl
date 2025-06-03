#include "ir_parser.h"

#include <stdlib.h>
#include <string.h>

#include "ir_tokens.h"
#include "nickl_impl.h"
#include "nkb/ir.h"
#include "nkb/types.h"
#include "nkl/common/token.h"
#include "nkl/core/lexer.h"
#include "nkl/core/nickl.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/list.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(ir_parser);

#define TRY(STMT)                \
    STMT;                        \
    do {                         \
        if (p->error_occurred) { \
            return ret;          \
        }                        \
    } while (0)

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

typedef struct SourceInfo {
    struct SourceInfo *next;

    NkAtom file;
    NkString text;

    NklToken const *cur_token;
} SourceInfo;

typedef struct {
    NklModule const mod;
    NkArena *const arena;
    char const **const token_names;
    NkIrSymbolDynArray *const ir;

    SourceInfo *src;

    NkArena scratch;

#define X(TYPE, VALUE_TYPE) NkIrType NK_CAT(_cached_, TYPE);
    NKIR_NUMERIC_ITERATE(X)
#undef X
    NkIrType _cached_void;

    NkIrParamArray proc_params;
    NkIrParam proc_ret;

    NkIrParamDynArray types;

    bool error_occurred;
} ParserState;

// TODO: Type ids are all zero

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

#define CACHED_TYPE(NAME, EXPR)                                \
    NkIrType NK_CAT(NK_CAT(get_, NAME), _t)(ParserState * p) { \
        NkIrType *cached = &p->NK_CAT(_cached_, NAME);         \
        if (!*cached) {                                        \
            *cached = (EXPR);                                  \
        }                                                      \
        return *cached;                                        \
    };

#define X(TYPE, VALUE_TYPE) CACHED_TYPE(TYPE, allocNumericType(p, VALUE_TYPE))
NKIR_NUMERIC_ITERATE(X)
#undef X

CACHED_TYPE(void, allocVoidType(p));

#undef CACHED_TYPE

static NkIrType allocStringType(ParserState *p, usize size) {
    NkIrAggregateElemInfo *elem = nk_arena_allocT(p->arena, NkIrAggregateElemInfo);
    *elem = (NkIrAggregateElemInfo){
        .type = get_i8_t(p),
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

NkIrType get_ptr_t(ParserState *p) {
    return get_i64_t(p); // TODO: Hardcoded ptr size
}

// TODO: Reuse some code between parsers?
// TODO: Improve error message accuracy

typedef struct {
} Void;
static Void const ret;

static void vreportError(ParserState *p, char const *fmt, va_list ap) {
    nickl_vreportError(
        p->mod->com->nkl,
        (NklSourceLocation){
            .file = nk_atom2s(p->src->file),
            .lin = p->src->cur_token->lin,
            .col = p->src->cur_token->col,
            .len = p->src->cur_token->len,
        },
        fmt,
        ap);
    p->error_occurred = true;
}

static NK_PRINTF_LIKE(2) void reportError(ParserState *p, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vreportError(p, fmt, ap);
    va_end(ap);
}

static bool on(ParserState const *p, u32 id) {
    return p->src->cur_token->id == id;
}

static NkString tokenStr(ParserState *p, NklToken const *token) {
    return nkl_getTokenStr(token, p->src->text);
}

static NkString curTokenStr(ParserState *p) {
    return tokenStr(p, p->src->cur_token);
}

static NkString escapedCurTokenStr(ParserState *p) {
    NkStringBuilder sb = (NkStringBuilder){.alloc = nk_arena_getAllocator(&p->scratch)};
    nks_escape(nksb_getStream(&sb), curTokenStr(p));
    return (NkString){NKS_INIT(sb)};
}

static void getTokenImpl(ParserState *p) {
    nk_assert(!on(p, NklToken_Eof));
    p->src->cur_token++;

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_printf(log, "next token: \"");
        nks_escape(log, curTokenStr(p));
        nk_printf(log, "\":%u", p->src->cur_token->id);
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
            nk_printf(log, "\":%u", p->src->cur_token->id);
        }

        getTokenImpl(p);
        return true;
    }
    return false;
}

static NklToken const *expect(ParserState *p, u32 id) {
    NklToken const *ret = p->src->cur_token;
    if (!ACCEPT(id)) {
        NkString const str = escapedCurTokenStr(p);
        ERROR(
            "unexpected token `" NKS_FMT "`, expected `%s`",
            NKS_ARG(str),
            p->token_names ? p->token_names[id] : "??TokenNameUnavailable??");
    }
    return ret;
}

static Void parseNumber(ParserState *p, void *addr, NkString str, NkIrNumericValueType value_type) {
    // TODO: Add support for hex constants

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

#define X(TYPE, VALUE_TYPE)                       \
    if (ACCEPT(NK_CAT(NklIrToken_, TYPE))) {      \
        return NK_CAT(NK_CAT(get_, TYPE), _t)(p); \
    }
    NKIR_NUMERIC_ITERATE(X)
#undef X

    if (ACCEPT(NklIrToken_void)) {
        return get_void_t(p);
    }

    if (ACCEPT(NklIrToken_LBrace)) {
        NkIrAggregateElemInfoDynArray elems = {.alloc = nk_arena_getAllocator(p->arena)};

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
                EXPECT(NklIrToken_Comma);
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

        NK_ITERATE(NkIrParam const *, rec, p->types) {
            if (rec->name == name) {
                return rec->type;
            }
        }

        p->src->cur_token--;
        ERROR("undefined type alias `" NKS_FMT "`", NKS_ARG(name_token_str));
    }

    ERROR_EXPECT("type");
}

static NkString parseString(ParserState *p) {
    NkString ret = {0};

    if (!on(p, NklToken_String) && !on(p, NklToken_EscapedString)) {
        ERROR("string expected");
    }

    u32 const token_id = p->src->cur_token->id;

    NkString const token_str = getToken(p);
    NkString const str = (NkString){token_str.data + 1, token_str.size - 2};

    if (token_id == NklToken_String) {
        char const *cstr = nk_tprintf(p->arena, NKS_FMT, NKS_ARG(str));
        return (NkString){cstr, str.size};
    } else {
        NkStringBuilder sb = {.alloc = nk_arena_getAllocator(p->arena)};
        nks_unescape(nksb_getStream(&sb), str);
        if (nks_last(sb)) {
            nksb_appendNull(&sb);
        }

        return (NkString){sb.data, sb.size - 1};
    }
}

static NkAtom parseId(ParserState *p) {
    NkAtom ret = 0;

    if (!on(p, NklToken_Id) && !on(p, NklIrToken_DollarTag)) {
        ERROR_EXPECT("identifier");
    }

    if (on(p, NklToken_Id)) {
        NkString const token_str = getToken(p);
        return nk_s2atom(token_str);
    }

    if (on(p, NklIrToken_DollarTag)) {
        NkString const token_str = getToken(p);
        return nk_s2atom((NkString){token_str.data + 1, token_str.size - 1});
    }

    return ret;
}

static NkIrRelocArray parseConst(ParserState *p, void *addr, NkIrType type) {
    NkIrRelocArray ret = {0};

    switch (type->kind) {
        case NkIrType_Aggregate: {
            EXPECT(NklIrToken_LBrace);

            NkIrRelocDynArray relocs = {.alloc = nk_arena_getAllocator(p->arena)};
            NK_ITERATE(NkIrAggregateElemInfo const *, elem, type->aggr) {
                usize offset = elem->offset;
                if (elem->type->kind == NkIrType_Numeric && elem->type->size == 1 &&
                    (on(p, NklToken_String) || on(p, NklToken_EscapedString))) {
                    TRY(NkString const str = parseString(p));
                    if (str.size + 1 != elem->count) {
                        p->src->cur_token--;
                        ERROR("invalid string length: expected %u characters, got %zu", elem->count, str.size + 1);
                    }
                    memcpy((u8 *)addr + offset, str.data, elem->count); // TODO: Extra copy
                    continue;
                }
                if (elem->count > 1) {
                    EXPECT(NklIrToken_LBracket);
                }
                for (usize i = 0; i < elem->count; i++) {
                    if (on(p, NklToken_Id) || on(p, NklIrToken_DollarTag)) {
                        if (elem->type->size != get_ptr_t(p)->size) {
                            ERROR("can only put reloc on a pointer type");
                        }
                        TRY(NkAtom const sym = parseId(p));
                        nkda_append(
                            &relocs,
                            ((NkIrReloc){
                                .sym = sym,
                                .offset = offset,
                            }));
                    } else {
                        TRY(NkIrRelocArray const relocs2 = parseConst(p, (u8 *)addr + offset, elem->type));
                        // TODO: Switch recursion to iteration to avoid merge?
                        NK_ITERATE(NkIrReloc const *, reloc, relocs2) {
                            nkda_append(
                                &relocs,
                                ((NkIrReloc){
                                    .sym = reloc->sym,
                                    .offset = offset + reloc->offset,
                                }));
                        }
                    }
                    offset += elem->type->size;
                    if (elem->count > 1) {
                        if (i == elem->count - 1) {
                            ACCEPT(NklIrToken_Comma);
                        } else {
                            EXPECT(NklIrToken_Comma);
                        }
                    }
                }
                if (elem->count > 1) {
                    EXPECT(NklIrToken_RBracket);
                }
                if (NK_INDEX(elem, type->aggr) == type->aggr.size - 1) {
                    ACCEPT(NklIrToken_Comma);
                } else {
                    EXPECT(NklIrToken_Comma);
                }
            }
            ret = (NkIrRelocArray){NK_SLICE_INIT(relocs)};

            EXPECT(NklIrToken_RBrace);

            break;
        }

        case NkIrType_Numeric: {
            if (NKIR_NUMERIC_IS_INT(type->num)) {
                TRY(NklToken const *token = expect(p, NklToken_Int));
                NkString const token_str = tokenStr(p, token);
                TRY(parseNumber(p, addr, token_str, type->num));
            } else {
                if (!on(p, NklToken_Int) && !on(p, NklToken_Float)) {
                    ERROR_EXPECT("numeric constant");
                }
                NkString const token_str = getToken(p);
                TRY(parseNumber(p, addr, token_str, type->num));
            }

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

    if (name == p->proc_ret.name) {
        is_param = true;
        type = get_ptr_t(p);
    } else {
        NK_ITERATE(NkIrParam const *, param, p->proc_params) {
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
    }

    if (!type) {
        p->src->cur_token--;
        ERROR("type must be specified");
    }

    if (is_param && to_write) {
        p->src->cur_token--;
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
            ERROR_EXPECT("local");
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
            type = get_i64_t(p);
        } else if (on(p, NklToken_Float)) {
            type = get_f64_t(p);
        }
    }

    if (on(p, NklIrToken_PercentTag)) {
        return parseLocal(p, type, false);
    }

    else if (on(p, NklToken_Id) || on(p, NklIrToken_DollarTag)) {
        TRY(NkAtom const sym = parseId(p));
        return nkir_makeRefGlobal(sym, get_ptr_t(p));
    }

    else if (type) {
        if (type->kind == NkIrType_Numeric) {
            NkIrImm imm = {0};
            TRY(parseConst(p, &imm, type));
            return nkir_makeRefImm(imm, type);
        } else {
            void *addr = NULL;
            if (type->size) {
                addr = nk_arena_allocAligned(p->arena, type->size, type->align);
                memset(addr, 0, type->size);
            }
            TRY(NkIrRelocArray relocs = parseConst(p, addr, type));

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

    ERROR_EXPECT("reference");
}

static NkIrRefArray parseRefArray(ParserState *p) {
    NkIrRefArray ret = {0};

    NkIrRefDynArray refs = {.alloc = nk_arena_getAllocator(p->arena)};

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

static NkIrLabel parseLabel(ParserState *p, bool allow_rel) {
    NkIrLabel ret = {0};

    if (on(p, NklIrToken_AtTag)) {
        NkString const token_str = getToken(p);
        NkAtom const label = nk_s2atom((NkString){token_str.data + 1, token_str.size - 1});
        return nkir_makeLabelAbs(label);
    } else if (allow_rel) {
        if (ACCEPT(NklIrToken_AtPlus)) {
            TRY(u32 offset = parseIdx(p));
            return nkir_makeLabelRel(offset);
        } else if (ACCEPT(NklIrToken_AtMinus)) {
            TRY(u32 offset = parseIdx(p));
            return nkir_makeLabelRel(-offset);
        }
    }

    if (allow_rel) {
        ERROR_EXPECT("label");
    } else {
        ERROR_EXPECT("absolute label");
    }
}

static NkIrInstr parseInstr(ParserState *p) {
    NkIrInstr ret = {0};

    if (ACCEPT(NklToken_Int)) {
        EXPECT(NklIrToken_Pipe);
    }

    if (on(p, NklIrToken_AtTag)) {
        TRY(NkIrLabel const label = parseLabel(p, false));
        ret = nkir_make_label(label.name);
    }

    else if (ACCEPT(NklIrToken_nop)) {
        ret = nkir_make_nop();
    }

    else if (ACCEPT(NklIrToken_ret)) {
        NkIrRef arg = {0};
        if (!on(p, NklToken_Newline)) {
            TRY(arg = parseRef(p, p->proc_ret.type));
        }
        ret = nkir_make_ret(arg);
    }

    else if (ACCEPT(NklIrToken_jmp)) {
        TRY(NkIrLabel const label = parseLabel(p, true));
        ret = nkir_make_jmp(label);
    } else if (ACCEPT(NklIrToken_jmpz)) {
        TRY(NkIrRef const cond = parseRef(p, NULL));
        EXPECT(NklIrToken_Comma);
        TRY(NkIrLabel const label = parseLabel(p, true));
        ret = nkir_make_jmpz(cond, label);
    } else if (ACCEPT(NklIrToken_jmpnz)) {
        TRY(NkIrRef const cond = parseRef(p, NULL));
        EXPECT(NklIrToken_Comma);
        TRY(NkIrLabel const label = parseLabel(p, true));
        ret = nkir_make_jmpnz(cond, label);
    }

    else if (ACCEPT(NklIrToken_call)) {
        TRY(NkIrRef const proc = parseRef(p, get_ptr_t(p)));
        EXPECT(NklIrToken_Comma);
        TRY(NkIrRefArray const args = parseRefArray(p));
        NkIrRef dst = nkir_makeRefNull(get_void_t(p));
        if (ACCEPT(NklIrToken_MinusGreater)) {
            TRY(dst = parseDst(p, NULL, true));
        }
        ret = nkir_make_call(dst, proc, args);
    }

    else if (ACCEPT(NklIrToken_store)) {
        TRY(NkIrRef const src = parseRef(p, NULL));
        EXPECT(NklIrToken_MinusGreater);
        TRY(NkIrRef const dst = parseRef(p, get_ptr_t(p)));
        ret = nkir_make_store(dst, src);
    } else if (ACCEPT(NklIrToken_load)) {
        TRY(NkIrRef const src = parseRef(p, get_ptr_t(p)));
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
#define CMP_IR(NAME)                                              \
    else if (ACCEPT(NK_CAT(NklIrToken_, NAME))) {                 \
        TRY(NkIrRef const lhs = parseRef(p, NULL));               \
        EXPECT(NklIrToken_Comma);                                 \
        TRY(NkIrRef const rhs = parseRef(p, lhs.type));           \
        EXPECT(NklIrToken_MinusGreater);                          \
        TRY(NkIrRef const dst = parseDst(p, get_i8_t(p), false)); \
        ret = NK_CAT(nkir_make_cmp_, NAME)(dst, lhs, rhs);        \
    }
#include "nkb/ir.inl"
    }

    else if (ACCEPT(NklIrToken_comment)) { // TODO: Pass comments from the lexer
        TRY(NkString const str = parseString(p));
        ret = nkir_make_comment(str);
    }

    else {
        ERROR_EXPECT("instruction");
    }

    EXPECT(NklToken_Newline);
    while (ACCEPT(NklToken_Newline)) {
    }

    return ret;
}

static Void parseProc(ParserState *p, NkIrVisibility vis) {
    TRY(NkAtom const name = parseId(p));

    EXPECT(NklIrToken_LParen);

    NkIrParamDynArray params = {.alloc = nk_arena_getAllocator(p->arena)};

    while (!on(p, NklIrToken_RParen) && !on(p, NklToken_Eof)) {
        EXPECT(NklIrToken_Colon);
        TRY(NkIrType const type = parseType(p));

        TRY(NklToken const *arg_name_token = expect(p, NklIrToken_PercentTag));
        NkString const arg_name_token_str = tokenStr(p, arg_name_token);
        NkAtom const arg_name = nk_s2atom((NkString){arg_name_token_str.data + 1, arg_name_token_str.size - 1});

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

    if (ACCEPT(NklIrToken_Colon)) {
        TRY(p->proc_ret.type = parseType(p));
    } else {
        p->proc_ret.type = get_void_t(p);
    }

    if (p->proc_ret.type->size > get_ptr_t(p)->size) {
        TRY(NklToken const *ret_ref_token = expect(p, NklIrToken_PercentTag));
        NkString const ret_ref_token_str = tokenStr(p, ret_ref_token);
        p->proc_ret.name = nk_s2atom((NkString){ret_ref_token_str.data + 1, ret_ref_token_str.size - 1});
    } else {
        p->proc_ret.name = 0;
    }

    EXPECT(NklIrToken_LBrace);

    EXPECT(NklToken_Newline);
    while (ACCEPT(NklToken_Newline)) {
    }

    NkIrInstrDynArray instrs = {.alloc = nk_arena_getAllocator(p->arena)};

    while (!on(p, NklIrToken_RBrace) && !on(p, NklToken_Eof)) {
        TRY(NkIrInstr const instr = parseInstr(p));
        nkda_append(&instrs, instr);
    }

    EXPECT(NklIrToken_RBrace);

    nkda_append(
        p->ir,
        ((NkIrSymbol){
            .proc =
                {
                    .params = p->proc_params,
                    .ret = p->proc_ret,
                    .instrs = {NK_SLICE_INIT(instrs)},
                    .flags = 0,
                },
            .name = name,
            .vis = vis,
            .flags = 0,
            .kind = NkIrSymbol_Proc,
        }));

    return ret;
}

static Void parseData(ParserState *p, NkIrVisibility vis, NkIrDataFlags flags) {
    TRY(NkAtom const name = parseId(p));

    NkIrType type = NULL;
    if (ACCEPT(NklIrToken_Colon)) {
        TRY(type = parseType(p));
    }

    if (!type) {
        if (on(p, NklToken_Int)) {
            type = get_i64_t(p);
        } else if (on(p, NklToken_Float)) {
            type = get_f64_t(p);
        }
    }

    void *addr = NULL;
    NkIrRelocArray relocs = {0};
    if (!on(p, NklToken_Newline)) {
        if (type) {
            if (type->size) {
                addr = nk_arena_allocAligned(p->arena, type->size, type->align);
                memset(addr, 0, type->size);
            }
            TRY(relocs = parseConst(p, addr, type));
        }

        else if (on(p, NklToken_String) || on(p, NklToken_EscapedString)) {
            TRY(NkString const str = parseString(p));

            type = allocStringType(p, str.size);
            addr = (void *)str.data;
        }

        else {
            ERROR_EXPECT("constant");
        }
    }

    if (!type) {
        ERROR_EXPECT("type or constant");
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
    TRY(NkString const lib_str = nickl_translateLib(p->mod, parseString(p)));
    NkAtom const lib = nk_s2atom(lib_str);

    TRY(NkAtom const sym_name = parseId(p));

    nkda_append(
        p->ir,
        ((NkIrSymbol){
            .extrn =
                {
                    .lib = lib,
                },
            .name = sym_name,
            .vis = 0,
            .flags = 0,
            .kind = NkIrSymbol_Extern,
        }));

    return ret;
}

static bool pushSource(ParserState *p, NkAtom file) {
    NkString text;
    if (!nickl_getText(p->mod->com->nkl, file, &text)) {
        p->error_occurred = true;
        return false;
    }

    NklTokenArray tokens;
    if (!nickl_getTokensIr(p->mod->com->nkl, file, &tokens)) {
        p->error_occurred = true;
        return false;
    }

    nk_assert(tokens.size && nk_slice_last(tokens).id == NklToken_Eof && "ill-formed token stream");

    SourceInfo *src = nk_arena_allocT(&p->scratch, SourceInfo);
    *src = (SourceInfo){
        .file = file,
        .text = text,
        .cur_token = tokens.data,
    };
    nk_list_push(p->src, src);

    return true;
}

static void popSource(ParserState *p) {
    nk_assert(p->src && "no source");
    nk_list_pop(p->src);
}

static Void parse(ParserState *p);

static Void parseSymbol(ParserState *p) {
    while (ACCEPT(NklToken_Newline)) {
    }

    NkIrVisibility vis = NkIrVisibility_Hidden;

    if (ACCEPT(NklIrToken_extern)) {
        TRY(parseExtern(p));
    }

    else if (ACCEPT(NklIrToken_type)) {
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
    }

    else if (ACCEPT(NklIrToken_include)) {
        TRY(NkString const name = parseString(p));
        NkAtom const file = nickl_findFile(p->mod->com->nkl, p->src->file, name);

        if (!file) {
            p->src->cur_token--;
            ERROR("file `" NKS_FMT "` not found", NKS_ARG(name));
        }

        TRY(pushSource(p, file));
        TRY(parse(p));
        popSource(p);
    }

    else {
        if (ACCEPT(NklIrToken_pub)) {
            vis = NkIrVisibility_Default;
        } else if (ACCEPT(NklIrToken_local)) {
            vis = NkIrVisibility_Local;
        }

        if (ACCEPT(NklIrToken_proc)) {
            TRY(parseProc(p, vis));
        } else if (ACCEPT(NklIrToken_const)) {
            TRY(parseData(p, vis, NkIrData_ReadOnly));
        } else if (ACCEPT(NklIrToken_data)) {
            TRY(parseData(p, vis, 0));
        } else {
            ERROR_EXPECT("symbol");
        }
    }

    EXPECT(NklToken_Newline);
    while (ACCEPT(NklToken_Newline)) {
    }

    return ret;
}

static Void parse(ParserState *p) {
    while (!on(p, NklToken_Eof)) {
        TRY(parseSymbol(p));
    }

    return ret;
}

bool nkl_ir_parse(NklIrParserData const *data, NkIrSymbolDynArray *out_syms) {
    NK_LOG_TRC("%s", __func__);

    usize const start_idx = out_syms->size;

    NklState nkl = data->mod->com->nkl;

    ParserState p = {
        .mod = data->mod,
        .arena = &nkl->arena,
        .token_names = data->token_names,
        .ir = out_syms,
        .types = {.alloc = nk_arena_getAllocator(p.arena)},
    };

    if (pushSource(&p, data->file)) {
        parse(&p);
    }

    nk_arena_free(&p.scratch);

    NK_LOG_STREAM_INF {
        NkArena scratch = {0};
        NkStream log = nk_log_getStream();
        nk_printf(log, "IR:\n");
        NkIrModule const syms = {out_syms->data + start_idx, out_syms->size - start_idx};
        nkir_inspectModule(log, &scratch, syms);
        nk_arena_free(&scratch);
    }

    return !p.error_occurred;
}
