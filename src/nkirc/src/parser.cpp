#include "parser.h"

#include <cstdlib>
#include <cstring>

#include "irc_impl.hpp"
#include "lexer.h"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "nkl/common/token.h"
#include "ntk/allocator.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "types.h"

namespace {

struct Void {};

NK_LOG_USE_SCOPE(parser);

#define LOG_TOKEN(ID) "(%s, \"%s\")", s_token_id[ID], s_token_text[ID]

#define CHECK(EXPR)         \
    EXPR;                   \
    if (m_error_occurred) { \
        return {};          \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))
#define ASSIGN(SLOT, VAL) CHECK(SLOT = (VAL))
#define APPEND(AR, VAL) CHECK(nkda_append((AR), (VAL)))
#define EXPECT(ID) CHECK(expect(ID))

static constexpr char const *c_entry_point_name = "main";

struct EmitterState {
    NkIrCompiler m_compiler;
    NkIrProg m_ir;
    NkIrModule m_mod;
    NkAtom m_file;
    NklTokenArray const m_tokens;
    NkString const m_text;

    NkAllocator m_file_alloc{nk_arena_getAllocator(&m_compiler->file_arena)};
    NkAllocator m_tmp_alloc{nk_arena_getAllocator(m_compiler->tmp_arena)};
    NkAllocator m_parse_alloc{nk_arena_getAllocator(&m_compiler->parse_arena)};

    NkString m_error_msg{};
    bool m_error_occurred{};

    NklToken const *m_cur_token{};
    ProcRecord *m_cur_proc{};

    Void emit() {
        nk_assert(m_tokens.size && nk_slice_last(m_tokens).id == t_eof && "ill-formed token stream");
        m_cur_token = &m_tokens.data[0];

        while (!check(t_eof)) {
            auto frame = nk_arena_grab(m_compiler->tmp_arena);
            defer {
                nk_arena_popFrame(m_compiler->tmp_arena, frame);
            };

            while (accept(t_newline)) {
            }

            if (accept(t_pub)) {
                if (accept(t_proc)) {
                    CHECK(parseProc(NkIrVisibility_Default));
                } else if (accept(t_const)) {
                    CHECK(parseData(NkIrVisibility_Default, true));
                } else if (accept(t_data)) {
                    CHECK(parseData(NkIrVisibility_Default, false));
                } else {
                    auto const token_str = nkl_getTokenStr(m_cur_token, m_text);
                    return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), Void{};
                }
            }

            else if (accept(t_local)) {
                if (accept(t_proc)) {
                    CHECK(parseProc(NkIrVisibility_Local));
                } else if (accept(t_const)) {
                    CHECK(parseData(NkIrVisibility_Local, true));
                } else if (accept(t_data)) {
                    CHECK(parseData(NkIrVisibility_Local, false));
                } else {
                    auto const token_str = nkl_getTokenStr(m_cur_token, m_text);
                    return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), Void{};
                }
            }

            else if (accept(t_extern)) {
                NkAtom lib = NK_ATOM_INVALID;

                if (check(t_string)) {
                    auto const lib_name = parseString(m_file_alloc);
                    if (lib_name == "c" || lib_name == "C") {
                        lib = m_compiler->conf.libc_name;
                    } else if (lib_name == "m" || lib_name == "M") {
                        lib = m_compiler->conf.libm_name;
                    } else if (lib_name == "pthread" || lib_name == "PTHREAD") {
                        lib = m_compiler->conf.libpthread_name;
                    } else {
                        lib = nk_s2atom(lib_name);
                    }
                }

                if (accept(t_proc)) {
                    CHECK(parseExternProc(lib));
                } else if (accept(t_data)) {
                    CHECK(parseExternData(lib));
                } else {
                    auto const token_str = nkl_getTokenStr(m_cur_token, m_text);
                    return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), Void{};
                }
            }

            else if (accept(t_proc)) {
                CHECK(parseProc(NkIrVisibility_Hidden));
            } else if (accept(t_type)) {
                CHECK(parseTypeDef());
            } else if (accept(t_const)) {
                CHECK(parseData(NkIrVisibility_Hidden, true));
            } else if (accept(t_data)) {
                CHECK(parseData(NkIrVisibility_Hidden, false));
            }

            else if (accept(t_include)) {
                if (!check(t_string)) {
                    return error("string constant expected in include"), Void{};
                }
                auto const str = parseString(m_file_alloc);
                if (!nkir_compileFile(m_compiler, nk_atom2s(m_file), str)) {
                    return error("failed to include file `" NKS_FMT "`", NKS_ARG(str)), Void{};
                }
            }

            else {
                auto const token_str = nkl_getTokenStr(m_cur_token, m_text);
                return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), Void{};
            }

            EXPECT(t_newline);
            while (accept(t_newline)) {
            }
        }

        return {};
    }

    Void parseProc(NkIrVisibility vis) {
        usize cur_line = m_cur_token->lin;

        DEFINE(sig, parseProcSignature(true, true));

        auto proc = nkir_createProc(m_ir);
        nkir_exportProc(m_ir, m_mod, proc);

        static auto const c_entry_point_id = nk_cs2atom(c_entry_point_name);
        if (sig.name == c_entry_point_id) {
            if (vis != NkIrVisibility_Default) {
                return error("entry point must be public"), Void{};
            }
            m_compiler->entry_point = proc;
        }

        nkir_startProc(
            m_ir,
            proc,
            {
                .name = sig.name,
                .proc_t = nkir_makeProcedureType(
                    m_compiler,
                    {
                        .args_t{NK_SLICE_INIT(sig.args_t)},
                        .ret_t = sig.ret_t,
                        .call_conv = sig.is_cdecl ? NkCallConv_Cdecl : NkCallConv_Nk,
                        .flags = (u8)(sig.is_variadic ? NkProcVariadic : 0),
                    }),
                .arg_names{NK_SLICE_INIT(sig.arg_names)},
                .file = m_file,
                .line = cur_line,
                .visibility = vis,
            });

        auto const decl = new (makeGlobalDecl(sig.name)) Decl{
            {.proc{
                .proc = proc,
                .locals = decltype(ProcRecord::locals)::create(m_parse_alloc),
                .labels = decltype(ProcRecord::labels)::create(m_parse_alloc),
            }},
            Decl_Proc,
        };
        m_cur_proc = &decl->as.proc;

        for (usize i = 0; i < sig.args_t.size; i++) {
            new (makeLocalDecl(sig.arg_names.data[i])) Decl{
                {.arg_index = i},
                Decl_Arg,
            };
        }

        EXPECT(t_brace_l);
        while (!check(t_brace_r) && !check(t_eof)) {
            while (accept(t_newline)) {
            }
            if (check(t_id)) {
                DEFINE(token, parseId());
                auto const name = nk_s2atom(nkl_getTokenStr(token, m_text));
                EXPECT(t_colon);
                DEFINE(type, parseType());
                new (makeLocalDecl(name)) Decl{
                    {.local = nkir_makeLocalVar(m_ir, name, type)},
                    Decl_LocalVar,
                };
            } else {
                nkir_setLine(m_ir, m_cur_token->lin);
                DEFINE(instr, parseInstr());
                nkir_emit(m_ir, instr);
            }
            EXPECT(t_newline);
            while (accept(t_newline)) {
            }
        }

        nkir_finishProc(m_ir, proc, m_cur_token->lin);

        EXPECT(t_brace_r);

        return {};
    }

    Void parseExternProc(NkAtom lib) {
        DEFINE(sig, parseProcSignature(true, false));

        new (makeGlobalDecl(sig.name)) Decl{
            {.extern_proc = nkir_makeExternProc(
                 m_ir,
                 lib,
                 sig.name,
                 nkir_makeProcedureType(
                     m_compiler,
                     NkIrProcInfo{
                         .args_t{NK_SLICE_INIT(sig.args_t)},
                         .ret_t = sig.ret_t,
                         .call_conv = NkCallConv_Cdecl,
                         .flags = (u8)(sig.is_variadic ? NkProcVariadic : 0),
                     }))},
            Decl_ExternProc,
        };

        return {};
    }

    Void parseTypeDef() {
        DEFINE(id_token, parseId());
        EXPECT(t_colon);
        DEFINE(type, parseType());
        auto name = nk_s2atom(nkl_getTokenStr(id_token, m_text));
        new (makeGlobalDecl(name)) Decl{
            {.type = type},
            Decl_Type,
        };

        return {};
    }

    Void parseData(NkIrVisibility vis, bool read_only) {
        DEFINE(id_token, parseId());
        EXPECT(t_colon);
        DEFINE(type, parseType());
        auto name = nk_s2atom(nkl_getTokenStr(id_token, m_text));
        auto decl = read_only ? nkir_makeRodata(m_ir, name, type, vis) : nkir_makeData(m_ir, name, type, vis);
        if (vis != NkIrVisibility_Local) {
            nkir_exportData(m_ir, m_mod, decl);
        }
        new (makeGlobalDecl(name)) Decl{
            {.data = decl},
            Decl_Data,
        };
        if (!check(t_newline)) {
            parseValue(nkir_makeDataRef(m_ir, decl), type);
        }
        return {};
    }

    Void parseExternData(NkAtom lib) {
        DEFINE(id_token, parseId());
        EXPECT(t_colon);
        DEFINE(type, parseType());
        auto name = nk_s2atom(nkl_getTokenStr(id_token, m_text));
        new (makeGlobalDecl(name)) Decl{
            {.extern_data = nkir_makeExternData(m_ir, lib, name, type)},
            Decl_ExternData,
        };
        return {};
    }

private:
    Decl *makeGlobalDecl(NkAtom name) {
        if (m_compiler->parser.decls.find(name)) {
            return error("identifier `%s` is already defined", nk_atom2cs(name)), nullptr;
        }
        auto const decl = nk_allocT<Decl>(m_parse_alloc);
        m_compiler->parser.decls.insert(name, decl);
        return decl;
    }

    Decl *makeLocalDecl(NkAtom name) {
        if (m_cur_proc->locals.find(name)) {
            return error("local `%s` is already defined", nk_atom2cs(name)), nullptr;
        }
        auto const decl = nk_allocT<Decl>(m_parse_alloc);
        m_cur_proc->locals.insert(name, decl);
        return decl;
    }

    NklToken const *parseId() {
        if (!check(t_id)) {
            return error("identifier expected"), nullptr;
        }
        NK_LOG_DBG("accept(id, \"" NKS_FMT "\")", NKS_ARG(nkl_getTokenStr(m_cur_token, m_text)));
        auto id = m_cur_token;
        getToken();
        return id;
    }

    Void parseNumeric(void *data, NkIrNumericValueType value_type) {
        if (NKIR_NUMERIC_IS_WHOLE(value_type)) {
            if (!check(t_int)) {
                return error("integer constant expected"), Void{};
            }
        } else {
            if (!check(t_float)) {
                return error("float constant expected"), Void{};
            }
        }

        auto const token_str = nkl_getTokenStr(m_cur_token, m_text);
        char const *cstr = nks_copyNt(m_tmp_alloc, token_str).data;
        getToken();

        char *endptr = NULL;

        switch (value_type) {
            case Int8:
                *(i8 *)data = strtol(cstr, &endptr, 10);
                break;
            case Uint8:
                *(u8 *)data = strtoul(cstr, &endptr, 10);
                break;
            case Int16:
                *(i16 *)data = strtol(cstr, &endptr, 10);
                break;
            case Uint16:
                *(u16 *)data = strtoul(cstr, &endptr, 10);
                break;
            case Int32:
                *(i32 *)data = strtol(cstr, &endptr, 10);
                break;
            case Uint32:
                *(u32 *)data = strtoul(cstr, &endptr, 10);
                break;
            case Int64:
                *(i64 *)data = strtoll(cstr, &endptr, 10);
                break;
            case Uint64:
                *(u64 *)data = strtoull(cstr, &endptr, 10);
                break;
            case Float32:
                *(f32 *)data = strtof(cstr, &endptr);
                break;
            case Float64:
                *(f64 *)data = strtod(cstr, &endptr);
                break;
            default:
                nk_assert(!"unreachable");
                break;
        }

        if (endptr != cstr + token_str.size) {
            return error("failed to parse numeric constant"), Void{};
        }

        return {};
    }

    NkString parseString(NkAllocator alloc) {
        auto const data = m_text.data + m_cur_token->pos + 1;
        auto const len = m_cur_token->len - 2;
        getToken();

        return nks_copyNt(alloc, {data, len});
    }

    NkString parseEscapedString(NkAllocator alloc) {
        auto const data = m_text.data + m_cur_token->pos + 1;
        auto const len = m_cur_token->len - 2;
        getToken();

        NkStringBuilder sb{};
        sb.alloc = alloc;
        nks_unescape(nksb_getStream(&sb), {data, len});
        nksb_appendNull(&sb);
        sb.size--;
        return {NKS_INIT(sb)};
    }

    struct ProcSignatureParseResult {
        NkDynArray(NkAtom) arg_names;
        NkDynArray(nktype_t) args_t;
        nktype_t ret_t{};
        NkAtom name{};
        bool is_variadic{};
        bool is_cdecl{};
    };

    ProcSignatureParseResult parseProcSignature(bool parse_name, bool parse_param_names) {
        ProcSignatureParseResult res{
            .arg_names{NKDA_INIT(m_parse_alloc)},
            .args_t{NKDA_INIT(m_file_alloc)},
        };
        if (accept(t_cdecl)) {
            res.is_cdecl = true;
        }
        if (parse_name) {
            DEFINE(id_token, parseId());
            res.name = nk_s2atom(nkl_getTokenStr(id_token, m_text));
        } else {
            res.name = NK_ATOM_INVALID;
        }
        EXPECT(t_par_l);
        do {
            if (check(t_par_r) || check(t_eof)) {
                break;
            }
            if (accept(t_period_3x)) {
                res.is_variadic = true;
                break;
            }
            NkAtom param_name = NK_ATOM_INVALID;
            if (parse_param_names) {
                DEFINE(id_token, parseId());
                param_name = nk_s2atom(nkl_getTokenStr(id_token, m_text));
                EXPECT(t_colon);
            }
            DEFINE(type, parseType());
            nkda_append(&res.args_t, type);
            nkda_append(&res.arg_names, param_name);
        } while (accept(t_comma));
        EXPECT(t_par_r);
        ASSIGN(res.ret_t, parseType());
        return res;
    }

    nktype_t parseType() {
        if (accept(t_void)) {
            return nkir_makeVoidType(m_compiler);
        } else if (accept(t_ptr)) {
            return nkir_makePointerType(m_compiler);
        }

        else if (accept(t_f32)) {
            return nkir_makeNumericType(m_compiler, Float32);
        } else if (accept(t_f64)) {
            return nkir_makeNumericType(m_compiler, Float64);
        } else if (accept(t_i16)) {
            return nkir_makeNumericType(m_compiler, Int16);
        } else if (accept(t_i32)) {
            return nkir_makeNumericType(m_compiler, Int32);
        } else if (accept(t_i64)) {
            return nkir_makeNumericType(m_compiler, Int64);
        } else if (accept(t_i8)) {
            return nkir_makeNumericType(m_compiler, Int8);
        } else if (accept(t_u16)) {
            return nkir_makeNumericType(m_compiler, Uint16);
        } else if (accept(t_u32)) {
            return nkir_makeNumericType(m_compiler, Uint32);
        } else if (accept(t_u64)) {
            return nkir_makeNumericType(m_compiler, Uint64);
        } else if (accept(t_u8)) {
            return nkir_makeNumericType(m_compiler, Uint8);
        }

        else if (check(t_id)) {
            DEFINE(id_token, parseId());
            auto const token_str = nkl_getTokenStr(id_token, m_text);
            auto name = nk_s2atom(token_str);
            auto found = m_compiler->parser.decls.find(name);
            if (!found) {
                return error("undeclared identifier `" NKS_FMT "`", NKS_ARG(token_str)), nullptr;
            } else if ((*found)->kind != Decl_Type) {
                return error("type expected"), nullptr;
            }
            return (*found)->as.type;
        }

        else if (accept(t_brace_l)) {
            NkDynArray(nktype_t) types{NKDA_INIT(m_tmp_alloc)};
            NkDynArray(usize) counts{NKDA_INIT(m_tmp_alloc)};

            do {
                if (check(t_par_r) || check(t_eof)) {
                    break;
                }
                if (accept(t_bracket_l)) {
                    if (!check(t_int)) {
                        return error("integer constant expected"), nullptr;
                    }
                    u64 count = 0;
                    CHECK(parseNumeric(&count, Uint64));
                    nkda_append(&counts, count);
                    EXPECT(t_bracket_r);
                } else {
                    nkda_append(&counts, 1ull);
                }
                APPEND(&types, parseType());
            } while (accept(t_comma));
            EXPECT(t_brace_r);
            return nkir_makeAggregateType(m_compiler, types.data, counts.data, types.size);
        }

        else if (check(t_par_l)) {
            DEFINE(sig, parseProcSignature(false, false));
            return nkir_makeProcedureType(
                m_compiler,
                {
                    .args_t{NK_SLICE_INIT(sig.args_t)},
                    .ret_t = sig.ret_t,
                    .call_conv = sig.is_cdecl ? NkCallConv_Cdecl : NkCallConv_Nk,
                    .flags = (u8)(sig.is_variadic ? NkProcVariadic : 0),
                });
        }

        else {
            auto const token_str = nkl_getTokenStr(m_cur_token, m_text);
            return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), nullptr;
        }
    }

    NkIrInstr parseInstr() {
        if (check(t_label)) {
            auto label = getLabel(nk_s2atom(nkl_getTokenStr(m_cur_token, m_text)));
            getToken();
            return nkir_make_label(label);
        }

        else if (accept(t_comment)) {
            if (check(t_string)) {
                return nkir_make_comment(m_ir, parseString(m_tmp_alloc));
            } else if (check(t_escaped_string)) {
                return nkir_make_comment(m_ir, parseEscapedString(m_tmp_alloc));
            } else {
                return error("string expected in comment"), NkIrInstr{};
            }
        }

        else if (accept(t_nop)) {
            return nkir_make_nop(m_ir);
        } else if (accept(t_ret)) {
            return nkir_make_ret(m_ir);
        }

        else if (accept(t_jmp)) {
            DEFINE(label, parseLabelRef());
            return nkir_make_jmp(m_ir, label);
        } else if (accept(t_jmpz)) {
            DEFINE(cond, parseRef());
            EXPECT(t_comma);
            DEFINE(label, parseLabelRef());
            return nkir_make_jmpz(m_ir, cond, label);
        } else if (accept(t_jmpnz)) {
            DEFINE(cond, parseRef());
            EXPECT(t_comma);
            DEFINE(label, parseLabelRef());
            return nkir_make_jmpnz(m_ir, cond, label);
        }

        else if (accept(t_call)) {
            NkIrRef dst{};
            DEFINE(proc, parseRef());
            EXPECT(t_comma);
            DEFINE(args, parseRefArray());
            if (accept(t_minus_greater)) {
                ASSIGN(dst, parseRef());
            }
            return nkir_make_call(m_ir, dst, proc, args);
        }

        else if (accept(t_ext)) {
            DEFINE(src, parseRef());
            EXPECT(t_minus_greater);
            DEFINE(dst, parseRef());
            return nkir_make_ext(m_ir, dst, src);
        } else if (accept(t_trunc)) {
            DEFINE(src, parseRef());
            EXPECT(t_minus_greater);
            DEFINE(dst, parseRef());
            return nkir_make_trunc(m_ir, dst, src);
        } else if (accept(t_fp2i)) {
            DEFINE(src, parseRef());
            EXPECT(t_minus_greater);
            DEFINE(dst, parseRef());
            return nkir_make_fp2i(m_ir, dst, src);
        } else if (accept(t_i2fp)) {
            DEFINE(src, parseRef());
            EXPECT(t_minus_greater);
            DEFINE(dst, parseRef());
            return nkir_make_i2fp(m_ir, dst, src);
        }

        else if (accept(t_neg)) {
            DEFINE(src, parseRef());
            EXPECT(t_minus_greater);
            DEFINE(dst, parseRef());
            return nkir_make_neg(m_ir, dst, src);
        } else if (accept(t_mov)) {
            DEFINE(src, parseRef());
            EXPECT(t_minus_greater);
            DEFINE(dst, parseRef());
            return nkir_make_mov(m_ir, dst, src);
        } else if (accept(t_lea)) {
            DEFINE(src, parseRef());
            EXPECT(t_minus_greater);
            DEFINE(dst, parseRef());
            return nkir_make_lea(m_ir, dst, src);
        }

        else if (false) {
        }
#define BIN_IR(NAME)                                          \
    else if (accept(NK_CAT(t_, NAME))) {                      \
        DEFINE(lhs, parseRef());                              \
        EXPECT(t_comma);                                      \
        DEFINE(rhs, parseRef());                              \
        EXPECT(t_minus_greater);                              \
        DEFINE(dst, parseRef());                              \
        return NK_CAT(nkir_make_, NAME)(m_ir, dst, lhs, rhs); \
    }
#include "nkb/ir.inl"

        else if (accept(t_cmp)) {
            if (false) {
            }
#define CMP_IR(NAME)                                              \
    else if (accept(NK_CAT(t_, NAME))) {                          \
        DEFINE(lhs, parseRef());                                  \
        EXPECT(t_comma);                                          \
        DEFINE(rhs, parseRef());                                  \
        EXPECT(t_minus_greater);                                  \
        DEFINE(dst, parseRef());                                  \
        return NK_CAT(nkir_make_cmp_, NAME)(m_ir, dst, lhs, rhs); \
    }
#include "nkb/ir.inl"
            else {
                auto const token_str = nkl_getTokenStr(m_cur_token, m_text);
                return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), NkIrInstr{};
            }
        }

        else if (accept(t_syscall)) {
            NkIrRef dst{};
            DEFINE(n, parseRef());
            EXPECT(t_comma);
            DEFINE(args, parseRefArray());
            EXPECT(t_minus_greater);
            ASSIGN(dst, parseRef());
            return nkir_make_syscall(m_ir, dst, n, args);
        }

        else {
            auto const token_str = nkl_getTokenStr(m_cur_token, m_text);
            return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), NkIrInstr{};
        }
    }

    Void parseValue(NkIrRef const &result_ref, nktype_t type) {
        switch (type->kind) {
            case NkIrType_Aggregate: {
                EXPECT(t_brace_l);

                for (usize i = 0; i < type->as.aggr.elems.size; i++) {
                    if (i) {
                        EXPECT(t_comma);
                    }

                    auto const &elem = type->as.aggr.elems.data[i];

                    if (elem.count > 1) {
                        EXPECT(t_bracket_l);
                    }

                    for (usize i = 0; i < elem.count; i++) {
                        if (i) {
                            EXPECT(t_comma);
                        }
                        auto ref = result_ref;
                        ref.post_offset += elem.offset + elem.type->size * i;
                        CHECK(parseValue(ref, elem.type));
                    }

                    if (elem.count > 1) {
                        accept(t_comma);
                        EXPECT(t_bracket_r);
                    }
                }

                accept(t_comma);
                EXPECT(t_brace_r);

                break;
            }

            case NkIrType_Numeric: {
                auto const data = nkir_dataRefDeref(m_ir, result_ref);
                CHECK(parseNumeric(data, type->as.num.value_type));
                break;
            }

            case NkIrType_Pointer:
            case NkIrType_Procedure:
            default:
                nk_assert(!"unreachable");
                return {};
        }

        return {};
    }

    NkIrData parseConst(nktype_t type) {
        auto const decl = nkir_makeRodata(m_ir, NK_ATOM_INVALID, type, NkIrVisibility_Local);
        CHECK(parseValue(nkir_makeDataRef(m_ir, decl), type));
        return decl;
    }

    NkIrRef parseRef() {
        NkIrRef result_ref{};
        u8 indir{};

        bool const deref = accept(t_amper);

        while (accept(t_bracket_l)) {
            indir++;
        }

        if (check(t_id)) {
            DEFINE(id_token, parseId());
            auto const token_str = nkl_getTokenStr(id_token, m_text);
            auto const name = nk_s2atom(token_str);
            auto decl = resolve(name);
            if (!decl) {
                return error("undeclared identifier `" NKS_FMT "`", NKS_ARG(token_str)), NkIrRef{};
            }
            switch (decl->kind) {
                case Decl_Arg:
                    result_ref = nkir_makeArgRef(m_ir, decl->as.arg_index);
                    break;
                case Decl_Proc:
                    result_ref = nkir_makeProcRef(m_ir, decl->as.proc.proc);
                    break;
                case Decl_LocalVar:
                    result_ref = nkir_makeFrameRef(m_ir, decl->as.local);
                    break;
                case Decl_Data:
                    result_ref = nkir_makeDataRef(m_ir, decl->as.data);
                    break;
                case Decl_ExternData:
                    result_ref = nkir_makeExternDataRef(m_ir, decl->as.extern_data);
                    break;
                case Decl_ExternProc:
                    result_ref = nkir_makeExternProcRef(m_ir, decl->as.extern_proc);
                    break;
                case Decl_Type:
                    return error("cannot reference a type"), NkIrRef{};
                default:
                    nk_assert(!"unreachable");
                    return {};
            }
        } else if (accept(t_ret)) {
            result_ref = nkir_makeRetRef(m_ir);
        }

        else if (check(t_string)) {
            auto const str = parseString(m_tmp_alloc);
            auto const str_t = nkir_makeArrayType(m_compiler, nkir_makeNumericType(m_compiler, Int8), str.size + 1);
            auto const decl = nkir_makeRodata(m_ir, NK_ATOM_INVALID, str_t, NkIrVisibility_Local);
            memcpy(nkir_getDataPtr(m_ir, decl), str.data, str_t->size);
            result_ref = nkir_makeDataRef(m_ir, decl);
        } else if (check(t_escaped_string)) {
            auto const str = parseEscapedString(m_tmp_alloc);
            auto const str_t = nkir_makeArrayType(m_compiler, nkir_makeNumericType(m_compiler, Int8), str.size + 1);
            auto const decl = nkir_makeRodata(m_ir, NK_ATOM_INVALID, str_t, NkIrVisibility_Local);
            memcpy(nkir_getDataPtr(m_ir, decl), str.data, str_t->size);
            result_ref = nkir_makeDataRef(m_ir, decl);
        }

        else if (check(t_int)) {
            auto const decl =
                nkir_makeRodata(m_ir, NK_ATOM_INVALID, nkir_makeNumericType(m_compiler, Int64), NkIrVisibility_Local);
            CHECK(parseNumeric(nkir_getDataPtr(m_ir, decl), Int64));
            result_ref = nkir_makeDataRef(m_ir, decl);
        } else if (check(t_f32)) {
            auto const decl =
                nkir_makeRodata(m_ir, NK_ATOM_INVALID, nkir_makeNumericType(m_compiler, Float64), NkIrVisibility_Local);
            CHECK(parseNumeric(nkir_getDataPtr(m_ir, decl), Float64));
            result_ref = nkir_makeDataRef(m_ir, decl);
        }

        else if (accept(t_colon)) {
            DEFINE(type, parseType());
            DEFINE(decl, parseConst(type));
            result_ref = nkir_makeDataRef(m_ir, decl);
        }

        else {
            auto const token_str = nkl_getTokenStr(m_cur_token, m_text);
            return error("unexpected token `" NKS_FMT "`", NKS_ARG(token_str)), NkIrRef{};
        }

        usize offset = 0;

        if (accept(t_plus)) {
            i64 value = 0;
            CHECK(parseNumeric(&value, Int64));
            offset = value;
        }

        if (indir && result_ref.type->kind != NkIrType_Pointer) {
            return error("dereference of a non-pointer type"), NkIrRef{};
        }

        for (u8 i = 0; i < indir; i++) {
            EXPECT(t_bracket_r);
        }

        if (accept(t_plus)) {
            i64 value = 0;
            CHECK(parseNumeric(&value, Int64));
            result_ref.post_offset += value;

            result_ref.offset += offset;
        } else {
            result_ref.post_offset += offset;
        }

        result_ref.indir += indir;

        if (indir) {
            EXPECT(t_colon);
            ASSIGN(result_ref.type, parseType());
        } else if (accept(t_colon)) {
            ASSIGN(result_ref.type, parseType());
        }

        if (indir && deref) {
            return error("cannot have both dereference and address of"), NkIrRef{};
        }

        if (deref) {
            result_ref = nkir_makeAddressRef(m_ir, result_ref, nkir_makePointerType(m_compiler));
        }

        return result_ref;
    }

    NkIrLabel parseLabelRef() {
        if (!check(t_label)) {
            return error("label expected"), NkIrLabel{};
        }
        auto label = getLabel(nk_s2atom(nkl_getTokenStr(m_cur_token, m_text)));
        getToken();
        return label;
    }

    NkIrLabel getLabel(NkAtom label_id) {
        auto found = m_cur_proc->labels.find(label_id);
        if (!found) {
            auto label = nkir_createLabel(m_ir, label_id);
            return m_cur_proc->labels.insert(label_id, label);
        } else {
            return *found;
        }
    }

    Decl *resolve(NkAtom name) {
        auto found = m_cur_proc->locals.find(name);
        if (found) {
            return *found;
        }
        found = m_compiler->parser.decls.find(name);
        if (found) {
            return *found;
        }
        return nullptr;
    }

    NkIrRefArray parseRefArray() {
        NkDynArray(NkIrRef) refs{NKDA_INIT(m_tmp_alloc)};
        EXPECT(t_par_l);
        do {
            if (check(t_par_r) || check(t_eof)) {
                break;
            }
            if (accept(t_period_3x)) {
                APPEND(&refs, nkir_makeVariadicMarkerRef(m_ir));
            } else {
                APPEND(&refs, parseRef());
            }
        } while (accept(t_comma));
        EXPECT(t_par_r);
        return {NK_SLICE_INIT(refs)};
    }

    void getToken() {
        nk_assert(m_cur_token->id != t_eof);
        m_cur_token++;
        NK_LOG_DBG("next token: " LOG_TOKEN(m_cur_token->id));
    }

    bool accept(ENkIrTokenId id) {
        if (check(id)) {
            NK_LOG_DBG("accept" LOG_TOKEN(id));
            getToken();
            return true;
        }
        return false;
    }

    bool check(ENkIrTokenId id) const {
        return m_cur_token->id == id;
    }

    void expect(ENkIrTokenId id) {
        if (!accept(id)) {
            auto const token_str = nkl_getTokenStr(m_cur_token, m_text);
            return error("expected `%s` before `" NKS_FMT "`", s_token_text[id], NKS_ARG(token_str));
        }
    }

    NK_PRINTF_LIKE(2) void error(char const *fmt, ...) {
        nk_assert(!m_error_occurred && "Parser error already initialized");

        va_list ap;
        va_start(ap, fmt);
        NkStringBuilder sb{};
        sb.alloc = m_tmp_alloc;
        nksb_vprintf(&sb, fmt, ap);
        m_error_msg = {NKS_INIT(sb)};
        va_end(ap);

        m_error_occurred = true;
    }
};

} // namespace

void nkir_parse(NkIrCompiler c, NkAtom file, NkString text, NklTokenArray tokens) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    c->parser.error_msg = {};
    c->parser.ok = true;

    EmitterState emitter{
        .m_compiler = c,
        .m_ir = c->ir,
        .m_mod = c->mod,
        .m_file = file,
        .m_tokens = tokens,
        .m_text = text,
    };

    emitter.emit();

    if (emitter.m_error_occurred) {
        c->parser.error_msg = emitter.m_error_msg;
        c->parser.error_token = *emitter.m_cur_token;
        c->parser.ok = false;
    }
}
