#include "parser.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <new>

#include "irc_impl.hpp"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/id.h"
#include "ntk/logger.h"
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
#define APPEND(AR, VAL) CHECK(nkar_append((AR), (VAL)))
#define EXPECT(ID) CHECK(expect(ID))

static constexpr char const *c_entry_point_name = "main";

struct GeneratorState {
    NkIrCompiler m_compiler;
    NkIrProg m_ir;
    nkid m_file;
    NkIrTokenView const m_tokens;

    NkAllocator m_file_alloc{nk_arena_getAllocator(&m_compiler->file_arena)};
    NkAllocator m_tmp_alloc{nk_arena_getAllocator(m_compiler->tmp_arena)};
    NkAllocator m_parse_alloc{nk_arena_getAllocator(&m_compiler->parse_arena)};

    nks m_error_msg{};
    bool m_error_occurred{};

    NkIrToken *m_cur_token{};
    ProcRecord *m_cur_proc{};

    Void generate() {
        assert(m_tokens.size && nkav_last(m_tokens).id == t_eof && "ill-formed token stream");
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
                    return error("unexpected token `" nks_Fmt "`", nks_Arg(m_cur_token->text)), Void{};
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
                    return error("unexpected token `" nks_Fmt "`", nks_Arg(m_cur_token->text)), Void{};
                }
            }

            else if (accept(t_extern)) {
                nkid lib = nk_invalid_id;

                if (check(t_string)) {
                    auto const lib_name = parseString(m_file_alloc);
                    if (lib_name == "c" || lib_name == "C") {
                        lib = m_compiler->conf.libc_name;
                    } else if (lib_name == "m" || lib_name == "M") {
                        lib = m_compiler->conf.libm_name;
                    } else if (lib_name == "pthread" || lib_name == "PTHREAD") {
                        lib = m_compiler->conf.libpthread_name;
                    } else {
                        lib = s2nkid(lib_name);
                    }
                }

                if (accept(t_proc)) {
                    CHECK(parseExternProc(lib));
                } else if (accept(t_data)) {
                    CHECK(parseExternData(lib));
                } else {
                    return error("unexpected token `" nks_Fmt "`", nks_Arg(m_cur_token->text)), Void{};
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
                if (!nkir_compileFile(m_compiler, nkid2s(m_file), str)) {
                    return error("failed to include file `" nks_Fmt "`", nks_Arg(str)), Void{};
                }
            }

            else {
                return error("unexpected token `" nks_Fmt "`", nks_Arg(m_cur_token->text)), Void{};
            }

            EXPECT(t_newline);
            while (accept(t_newline)) {
            }
        }

        return {};
    }

    Void parseProc(NkIrVisibility vis) {
        size_t cur_line = m_cur_token->lin;

        DEFINE(sig, parseProcSignature(true));

        auto proc = nkir_createProc(m_ir);

        static auto const c_entry_point_id = cs2nkid(c_entry_point_name);
        if (sig.name == c_entry_point_id) {
            if (vis != NkIrVisibility_Default) {
                return error("entry point must be public"), Void{};
            }
            m_compiler->entry_point = proc;
        }

        nkir_startProc(
            m_ir,
            proc,
            sig.name,
            nkir_makeProcedureType(
                m_compiler,
                {
                    .args_t{nkav_init(sig.args_t)},
                    .ret_t = sig.ret_t,
                    .call_conv = sig.is_cdecl ? NkCallConv_Cdecl : NkCallConv_Nk,
                    .flags = (uint8_t)(sig.is_variadic ? NkProcVariadic : 0),
                }),
            {nkav_init(sig.arg_names)},
            m_file,
            cur_line,
            vis);

        auto const decl = new (makeGlobalDecl(sig.name)) Decl{
            {.proc{
                .proc = proc,
                .locals = decltype(ProcRecord::locals)::create(m_parse_alloc),
                .labels = decltype(ProcRecord::labels)::create(m_parse_alloc),
            }},
            Decl_Proc,
        };
        m_cur_proc = &decl->as.proc;

        for (size_t i = 0; i < sig.args_t.size; i++) {
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
                auto const name = s2nkid(token->text);
                EXPECT(t_colon);
                DEFINE(type, parseType());
                new (makeLocalDecl(name)) Decl{
                    {.local = nkir_makeLocalVar(m_ir, name, type)},
                    Decl_LocalVar,
                };
            } else {
                nkir_setLine(m_ir, m_cur_token->lin);
                DEFINE(instr, parseInstr());
                nkir_gen(m_ir, {&instr, 1});
            }
            EXPECT(t_newline);
            while (accept(t_newline)) {
            }
        }

        nkir_finishProc(m_ir, proc, m_cur_token->lin);

        EXPECT(t_brace_r);

        return {};
    }

    Void parseExternProc(nkid lib) {
        DEFINE(sig, parseProcSignature(false));

        new (makeGlobalDecl(sig.name)) Decl{
            {.extern_proc = nkir_makeExternProc(
                 m_ir,
                 lib,
                 sig.name,
                 nkir_makeProcedureType(
                     m_compiler,
                     NkIrProcInfo{
                         .args_t{nkav_init(sig.args_t)},
                         .ret_t = sig.ret_t,
                         .call_conv = NkCallConv_Cdecl,
                         .flags = (uint8_t)(sig.is_variadic ? NkProcVariadic : 0),
                     }))},
            Decl_ExternProc,
        };

        return {};
    }

    Void parseTypeDef() {
        DEFINE(token, parseId());
        EXPECT(t_colon);
        DEFINE(type, parseType());
        auto name = s2nkid(token->text);
        new (makeGlobalDecl(name)) Decl{
            {.type = type},
            Decl_Type,
        };

        return {};
    }

    Void parseData(NkIrVisibility vis, bool read_only) {
        DEFINE(token, parseId());
        EXPECT(t_colon);
        DEFINE(type, parseType());
        auto name = s2nkid(token->text);
        auto decl = read_only ? nkir_makeRodata(m_ir, name, type, vis) : nkir_makeData(m_ir, name, type, vis);
        new (makeGlobalDecl(name)) Decl{
            {.data = decl},
            Decl_Data,
        };
        if (!check(t_newline)) {
            parseValue(nkir_makeDataRef(m_ir, decl), type);
        }
        return {};
    }

    Void parseExternData(nkid lib) {
        DEFINE(token, parseId());
        EXPECT(t_colon);
        DEFINE(type, parseType());
        auto name = s2nkid(token->text);
        new (makeGlobalDecl(name)) Decl{
            {.extern_data = nkir_makeExternData(m_ir, lib, s2nkid(token->text), type)},
            Decl_ExternData,
        };
        return {};
    }

private:
    Decl *makeGlobalDecl(nkid name) {
        if (m_compiler->parser.decls.find(name)) {
            return error("identifier `%s` is already defined", nkid2cs(name)), nullptr;
        }
        auto const decl = nk_alloc_t<Decl>(m_parse_alloc);
        m_compiler->parser.decls.insert(name, decl);
        return decl;
    }

    Decl *makeLocalDecl(nkid name) {
        if (m_cur_proc->locals.find(name)) {
            return error("local `%s` is already defined", nkid2cs(name)), nullptr;
        }
        auto const decl = nk_alloc_t<Decl>(m_parse_alloc);
        m_cur_proc->locals.insert(name, decl);
        return decl;
    }

    NkIrToken *parseId() {
        if (!check(t_id)) {
            return error("identifier expected"), nullptr;
        }
        NK_LOG_DBG("accept(id, \"" nks_Fmt "\")", nks_Arg(m_cur_token->text));
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

        nks str = m_cur_token->text;
        char const *cstr = nk_strcpy_nt(m_tmp_alloc, str).data;
        getToken();

        char *endptr = NULL;

        switch (value_type) {
        case Int8:
            *(int8_t *)data = strtol(cstr, &endptr, 10);
            break;
        case Uint8:
            *(uint8_t *)data = strtoul(cstr, &endptr, 10);
            break;
        case Int16:
            *(int16_t *)data = strtol(cstr, &endptr, 10);
            break;
        case Uint16:
            *(uint16_t *)data = strtoul(cstr, &endptr, 10);
            break;
        case Int32:
            *(int32_t *)data = strtol(cstr, &endptr, 10);
            break;
        case Uint32:
            *(uint32_t *)data = strtoul(cstr, &endptr, 10);
            break;
        case Int64:
            *(int64_t *)data = strtoll(cstr, &endptr, 10);
            break;
        case Uint64:
            *(uint64_t *)data = strtoull(cstr, &endptr, 10);
            break;
        case Float32:
            *(float *)data = strtof(cstr, &endptr);
            break;
        case Float64:
            *(double *)data = strtod(cstr, &endptr);
            break;
        default:
            assert(!"unreachable");
            break;
        }

        if (endptr != cstr + str.size) {
            return error("failed to parse numeric constant"), Void{};
        }

        return {};
    }

    nks parseString(NkAllocator alloc) {
        auto const data = m_cur_token->text.data + 1;
        auto const len = m_cur_token->text.size - 2;
        getToken();

        return nk_strcpy_nt(alloc, {data, len});
    }

    nks parseEscapedString(NkAllocator alloc) {
        auto const data = m_cur_token->text.data + 1;
        auto const len = m_cur_token->text.size - 2;
        getToken();

        NkStringBuilder sb{};
        sb.alloc = alloc;
        nks_unescape(nksb_getStream(&sb), {data, len});
        nksb_append_null(&sb);
        sb.size--;
        return {nkav_init(sb)};
    }

    struct ProcSignatureParseResult {
        nkar_type(nkid) arg_names;
        nkar_type(nktype_t) args_t;
        nktype_t ret_t{};
        nkid name{};
        bool is_variadic{};
        bool is_cdecl{};
    };

    ProcSignatureParseResult parseProcSignature(bool parse_names) {
        ProcSignatureParseResult res{
            .arg_names{0, 0, 0, m_parse_alloc},
            .args_t{0, 0, 0, m_file_alloc},
        };
        if (accept(t_cdecl)) {
            res.is_cdecl = true;
        }
        DEFINE(name, parseId());
        res.name = s2nkid(name->text);
        EXPECT(t_par_l);
        do {
            if (check(t_par_r) || check(t_eof)) {
                break;
            }
            if (accept(t_period_3x)) {
                res.is_variadic = true;
                break;
            }
            nkid name = nk_invalid_id;
            if (parse_names) {
                DEFINE(token, parseId());
                name = s2nkid(token->text);
                EXPECT(t_colon);
            }
            DEFINE(type, parseType());
            nkar_append(&res.args_t, type);
            nkar_append(&res.arg_names, name);
        } while (accept(t_comma));
        EXPECT(t_par_r);
        ASSIGN(res.ret_t, parseType());
        return res;
    }

    nktype_t parseType() {
        if (accept(t_void)) {
            return nkir_makeVoidType(m_compiler);
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
            DEFINE(token, parseId());
            auto name = s2nkid(token->text);
            auto found = m_compiler->parser.decls.find(name);
            if (!found) {
                return error("undeclared identifier `" nks_Fmt "`", nks_Arg(token->text)), nullptr;
            } else if ((*found)->kind != Decl_Type) {
                return error("type expected"), nullptr;
            }
            return (*found)->as.type;
        }

        else if (accept(t_brace_l)) {
            nkar_type(nktype_t) types{0, 0, 0, m_tmp_alloc};
            nkar_type(size_t) counts{0, 0, 0, m_tmp_alloc};

            do {
                if (check(t_par_r) || check(t_eof)) {
                    break;
                }
                if (accept(t_bracket_l)) {
                    if (!check(t_int)) {
                        return error("integer constant expected"), nullptr;
                    }
                    uint64_t count = 0;
                    CHECK(parseNumeric(&count, Uint64));
                    nkar_append(&counts, count);
                    EXPECT(t_bracket_r);
                } else {
                    nkar_append(&counts, 1ull);
                }
                APPEND(&types, parseType());
            } while (accept(t_comma));
            EXPECT(t_brace_r);
            return nkir_makeAggregateType(m_compiler, types.data, counts.data, types.size);
        }

        else if (accept(t_aster)) {
            DEFINE(target_type, parseType());
            return nkir_makePointerType(m_compiler, target_type);
        }

        else {
            return error("unexpected token `" nks_Fmt "`", nks_Arg(m_cur_token->text)), nullptr;
        }
    }

    NkIrInstr parseInstr() {
        if (check(t_label)) {
            auto label = getLabel(s2nkid(m_cur_token->text));
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
#define BIN_IR(NAME)                                       \
    else if (accept(CAT(t_, NAME))) {                      \
        DEFINE(lhs, parseRef());                           \
        EXPECT(t_comma);                                   \
        DEFINE(rhs, parseRef());                           \
        EXPECT(t_minus_greater);                           \
        DEFINE(dst, parseRef());                           \
        return CAT(nkir_make_, NAME)(m_ir, dst, lhs, rhs); \
    }
#include "nkb/ir.inl"

        else if (accept(t_cmp)) {
            if (false) {
            }
#define CMP_IR(NAME)                                           \
    else if (accept(CAT(t_, NAME))) {                          \
        DEFINE(lhs, parseRef());                               \
        EXPECT(t_comma);                                       \
        DEFINE(rhs, parseRef());                               \
        EXPECT(t_minus_greater);                               \
        DEFINE(dst, parseRef());                               \
        return CAT(nkir_make_cmp_, NAME)(m_ir, dst, lhs, rhs); \
    }
#include "nkb/ir.inl"
            else {
                return error("unexpected token `" nks_Fmt "`", nks_Arg(m_cur_token->text)), NkIrInstr{};
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
            return error("unexpected token `" nks_Fmt "`", nks_Arg(m_cur_token->text)), NkIrInstr{};
        }
    }

    Void parseValue(NkIrRef const &result_ref, nktype_t type) {
        switch (type->kind) {
        case NkType_Aggregate: {
            EXPECT(t_brace_l);

            for (size_t i = 0; i < type->as.aggr.elems.size; i++) {
                if (i) {
                    EXPECT(t_comma);
                }

                auto const &elem = type->as.aggr.elems.data[i];

                if (elem.count > 1) {
                    EXPECT(t_bracket_l);
                }

                for (size_t i = 0; i < elem.count; i++) {
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

        case NkType_Numeric: {
            auto const data = nkir_dataRefDeref(m_ir, result_ref);
            CHECK(parseNumeric(data, type->as.num.value_type));
            break;
        }

        case NkType_Pointer:
        case NkType_Procedure:
        default:
            assert(!"unreachable");
            return {};
        }

        return {};
    }

    NkIrData parseConst(nkid name, nktype_t type, NkIrVisibility vis) {
        auto const decl = nkir_makeRodata(m_ir, name, type, vis);
        CHECK(parseValue(nkir_makeDataRef(m_ir, decl), type));
        return decl;
    }

    NkIrRef parseRef() {
        NkIrRef result_ref{};
        uint8_t indir{};

        bool const deref = accept(t_amper);

        while (accept(t_bracket_l)) {
            indir++;
        }

        if (check(t_id)) {
            DEFINE(token, parseId());
            auto const name = s2nkid(token->text);
            auto decl = resolve(name);
            if (!decl) {
                return error("undeclared identifier `" nks_Fmt "`", nks_Arg(token->text)), NkIrRef{};
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
                assert(!"unreachable");
                return {};
            }
        } else if (accept(t_ret)) {
            result_ref = nkir_makeRetRef(m_ir);
        }

        else if (check(t_string)) {
            auto const str = parseString(m_tmp_alloc);
            auto const str_t = nkir_makeArrayType(m_compiler, nkir_makeNumericType(m_compiler, Int8), str.size + 1);
            auto const decl = nkir_makeRodata(m_ir, nk_invalid_id, str_t, NkIrVisibility_Local);
            memcpy(nkir_getDataPtr(m_ir, decl), str.data, str_t->size);
            result_ref = nkir_makeDataRef(m_ir, decl);
        } else if (check(t_escaped_string)) {
            auto const str = parseEscapedString(m_tmp_alloc);
            auto const str_t = nkir_makeArrayType(m_compiler, nkir_makeNumericType(m_compiler, Int8), str.size + 1);
            auto const decl = nkir_makeRodata(m_ir, nk_invalid_id, str_t, NkIrVisibility_Local);
            memcpy(nkir_getDataPtr(m_ir, decl), str.data, str_t->size);
            result_ref = nkir_makeDataRef(m_ir, decl);
        }

        else if (check(t_int)) {
            auto const decl =
                nkir_makeRodata(m_ir, nk_invalid_id, nkir_makeNumericType(m_compiler, Int64), NkIrVisibility_Local);
            CHECK(parseNumeric(nkir_getDataPtr(m_ir, decl), Int64));
            result_ref = nkir_makeDataRef(m_ir, decl);
        } else if (check(t_float)) {
            auto const decl =
                nkir_makeRodata(m_ir, nk_invalid_id, nkir_makeNumericType(m_compiler, Float64), NkIrVisibility_Local);
            CHECK(parseNumeric(nkir_getDataPtr(m_ir, decl), Float64));
            result_ref = nkir_makeDataRef(m_ir, decl);
        }

        else if (accept(t_colon)) {
            DEFINE(type, parseType());
            DEFINE(decl, parseConst(nk_invalid_id, type, NkIrVisibility_Local));
            result_ref = nkir_makeDataRef(m_ir, decl);
        }

        else {
            return error("unexpected token `" nks_Fmt "`", nks_Arg(m_cur_token->text)), NkIrRef{};
        }

        size_t offset = 0;

        if (accept(t_plus)) {
            int64_t value = 0;
            CHECK(parseNumeric(&value, Int64));
            offset = value;
        }

        for (uint8_t i = 0; i < indir; i++) {
            EXPECT(t_bracket_r);

            if (result_ref.type->kind != NkType_Pointer) {
                return error("dereference of a non-pointer type"), NkIrRef{};
            }
            result_ref.type = result_ref.type->as.ptr.target_type;
        }

        if (accept(t_plus)) {
            int64_t value = 0;
            CHECK(parseNumeric(&value, Int64));
            result_ref.post_offset += value;

            result_ref.offset += offset;
        } else {
            result_ref.post_offset += offset;
        }

        result_ref.indir += indir;

        if (accept(t_colon)) {
            ASSIGN(result_ref.type, parseType());
        }

        if (indir && deref) {
            return error("cannot have both dereference and address of"), NkIrRef{};
        }

        if (deref) {
            result_ref = nkir_makeAddressRef(m_ir, result_ref, nkir_makePointerType(m_compiler, result_ref.type));
        }

        return result_ref;
    }

    NkIrLabel parseLabelRef() {
        if (!check(t_label)) {
            return error("label expected"), NkIrLabel{};
        }
        auto label = getLabel(s2nkid(m_cur_token->text));
        getToken();
        return label;
    }

    NkIrLabel getLabel(nkid label_id) {
        auto found = m_cur_proc->labels.find(s2nkid(m_cur_token->text));
        if (!found) {
            auto label = nkir_createLabel(m_ir, s2nkid(m_cur_token->text));
            return m_cur_proc->labels.insert(label_id, label);
        } else {
            return *found;
        }
    }

    Decl *resolve(nkid name) {
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
        nkar_type(NkIrRef) refs{0, 0, 0, m_tmp_alloc};
        EXPECT(t_par_l);
        do {
            if (check(t_par_r) || check(t_eof)) {
                break;
            }
            APPEND(&refs, parseRef());
        } while (accept(t_comma));
        EXPECT(t_par_r);
        return {nkav_init(refs)};
    }

    void getToken() {
        assert(m_cur_token->id != t_eof);
        m_cur_token++;
        NK_LOG_DBG("next token: " LOG_TOKEN(m_cur_token->id));
    }

    bool accept(ETokenId id) {
        if (check(id)) {
            NK_LOG_DBG("accept" LOG_TOKEN(id));
            getToken();
            return true;
        }
        return false;
    }

    bool check(ETokenId id) const {
        return m_cur_token->id == id;
    }

    void expect(ETokenId id) {
        if (!accept(id)) {
            return error("expected `%s` before `" nks_Fmt "`", s_token_text[id], nks_Arg(m_cur_token->text));
        }
    }

    NK_PRINTF_LIKE(2, 3) void error(char const *fmt, ...) {
        assert(!m_error_occurred && "Parser error already initialized");

        va_list ap;
        va_start(ap, fmt);
        NkStringBuilder sb{};
        sb.alloc = m_tmp_alloc;
        nksb_vprintf(&sb, fmt, ap);
        m_error_msg = {nkav_init(sb)};
        va_end(ap);

        m_error_occurred = true;
    }
};

} // namespace

void nkir_parse(NkIrCompiler c, nkid file, NkIrTokenView tokens) {
    NK_LOG_TRC("%s", __func__);

    c->parser.error_msg = {};
    c->parser.ok = true;

    GeneratorState gen{
        .m_compiler = c,
        .m_ir = c->ir,
        .m_file = file,
        .m_tokens = tokens,
    };

    gen.generate();

    if (gen.m_error_occurred) {
        c->parser.error_msg = gen.m_error_msg;
        c->parser.error_token = *gen.m_cur_token;
        c->parser.ok = false;
        return;
    }
}
