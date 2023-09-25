#include "parser.hpp"

#include <cassert>
#include <cstring>
#include <new>

#include "nk/common/array.hpp"
#include "nk/common/hash_map.hpp"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/string.hpp"
#include "nk/common/string_builder.h"
#include "nkb/common.h"
#include "nkb/ir.h"

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
#define APPEND(AR, VAL) CHECK(AR.emplace(VAL))
#define EXPECT(ID) CHECK(expect(ID))

#define SCNf32 "f"
#define SCNf64 "lf"

static constexpr char const *c_entry_point_name = "main";

struct GeneratorState {
    NkIrProg &m_ir;
    NkIrProc &m_entry_point;
    NkSlice<NkIrToken> const m_tokens;

    NkIrTypeCache *m_types;

    NkArena *m_file_arena;
    NkArena *m_tmp_arena;
    NkArena m_parse_arena{};

    NkAllocator m_file_alloc{nk_arena_getAllocator(m_file_arena)};
    NkAllocator m_tmp_alloc{nk_arena_getAllocator(m_tmp_arena)};
    NkAllocator m_parse_alloc{nk_arena_getAllocator(&m_parse_arena)};

    nkstr m_error_msg{};
    bool m_error_occurred{};

    NkIrToken *m_cur_token{};

    struct Decl;

    struct ProcRecord {
        NkIrProc proc;
        NkHashMap<nkid, Decl *> locals;
        NkHashMap<nkid, NkIrLabel> labels;
    };

    enum EDeclKind {
        Decl_None,

        Decl_Arg,
        Decl_Proc,
        Decl_LocalVar,
        Decl_GlobalVar,
        Decl_ExternData,
        Decl_ExternProc,

        Decl_Type,
        Decl_Const
    };

    struct Decl {
        union {
            size_t arg_index;
            ProcRecord proc;
            NkIrLocalVar local;
            NkIrGlobalVar global;
            NkIrExternData extern_data;
            NkIrExternProc extern_proc;
            nktype_t type;
            NkIrConst cnst;
        } as;
        EDeclKind kind;
    };

    NkHashMap<nkid, Decl *> m_decls{};
    ProcRecord *m_cur_proc{};

    Void generate() {
        m_decls = decltype(m_decls)::create(m_parse_alloc);

        assert(m_tokens.size() && m_tokens.back().id == t_eof && "ill-formed token stream");
        m_cur_token = &m_tokens[0];

        while (!check(t_eof)) {
            while (accept(t_newline)) {
            }

            if (check(t_proc)) {
                DEFINE(sig, parseProcSignature());

                if (sig.is_extern) {
                    new (makeGlobalDecl(s2nkid(sig.name))) Decl{
                        {.extern_proc = nkir_makeExternProc(
                             m_ir,
                             sig.name,
                             nkir_makeProcedureType(
                                 m_types,
                                 {
                                     .args_t{sig.args_t.data(), sig.args_t.size()},
                                     .ret_t{sig.ret_t.data(), sig.ret_t.size()},
                                     .call_conv = NkCallConv_Cdecl,
                                     .flags = (uint8_t)(sig.is_variadic ? NkProcVariadic : 0),
                                 }))},
                        Decl_ExternProc,
                    };
                } else {
                    auto proc = nkir_createProc(m_ir);

                    static auto const c_entry_point_id = cs2nkid(c_entry_point_name);
                    if (s2nkid(sig.name) == c_entry_point_id) {
                        m_entry_point = proc;
                    }

                    nkir_startProc(
                        m_ir,
                        proc,
                        sig.name,
                        nkir_makeProcedureType(
                            m_types,
                            {
                                .args_t{sig.args_t.data(), sig.args_t.size()},
                                .ret_t{sig.ret_t.data(), sig.ret_t.size()},
                                .call_conv = sig.is_cdecl ? NkCallConv_Cdecl : NkCallConv_Nk,
                                .flags = (uint8_t)(sig.is_variadic ? NkProcVariadic : 0),
                            }));

                    auto const decl = new (makeGlobalDecl(s2nkid(sig.name))) Decl{
                        {.proc{
                            .proc = proc,
                            .locals = decltype(ProcRecord::locals)::create(m_parse_alloc),
                            .labels = decltype(ProcRecord::labels)::create(m_parse_alloc),
                        }},
                        Decl_Proc,
                    };
                    m_cur_proc = &decl->as.proc;

                    for (size_t i = 0; i < sig.args_t.size(); i++) {
                        new (makeLocalDecl(sig.arg_names[i])) Decl{
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
                                {.local = nkir_makeLocalVar(m_ir, type)},
                                Decl_LocalVar,
                            };
                        } else {
                            DEFINE(instr, parseInstr());
                            nkir_gen(m_ir, {&instr, 1});
                        }
                        EXPECT(t_newline);
                        while (accept(t_newline)) {
                        }
                    }
                    EXPECT(t_brace_r);
                }
            } else if (accept(t_type)) {
                DEFINE(token, parseId());
                EXPECT(t_colon);
                DEFINE(type, parseType());
                auto name = s2nkid(token->text);
                new (makeGlobalDecl(name)) Decl{
                    {.type = type},
                    Decl_Type,
                };
            } else if (accept(t_const)) {
                DEFINE(token, parseId());
                EXPECT(t_colon);
                DEFINE(type, parseType());
                DEFINE(cnst, parseConst(type));
                auto name = s2nkid(token->text);
                new (makeGlobalDecl(name)) Decl{
                    {.cnst = cnst},
                    Decl_Const,
                };
            } else if (accept(t_data)) {
                bool is_extern = accept(t_extern);
                DEFINE(token, parseId());
                EXPECT(t_colon);
                DEFINE(type, parseType());
                auto name = s2nkid(token->text);
                if (is_extern) {
                    new (makeGlobalDecl(name)) Decl{
                        {.extern_data = nkir_makeExternData(m_ir, token->text, type)},
                        Decl_ExternData,
                    };
                } else {
                    new (makeGlobalDecl(name)) Decl{
                        {.global = nkir_makeGlobalVar(m_ir, type)},
                        Decl_GlobalVar,
                    };
                }
            } else {
                return error("unexpected token `%.*s`", (int)m_cur_token->text.size, m_cur_token->text.data), Void{};
            }

            EXPECT(t_newline);
            while (accept(t_newline)) {
            }

            nk_arena_clear(m_tmp_arena);
        }

        if (m_entry_point.id == INVALID_ID) {
            return error("entry point `%s` is not defined", c_entry_point_name), Void{};
        }

        return {};
    }

private:
    Decl *makeGlobalDecl(nkid name) {
        if (m_decls.find(name)) {
            return error("global `%s` is already defined", nkid2cs(name)), nullptr;
        }
        auto const decl = nk_alloc_t<Decl>(m_parse_alloc);
        m_decls.insert(name, decl);
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
        NK_LOG_DBG("accept(id, \"%.*s\")", (int)m_cur_token->text.size, m_cur_token->text.data);
        auto id = m_cur_token;
        getToken();
        return id;
    }

    int64_t parseInt() {
        if (!check(t_int)) {
            return error("integer constant expected"), 0;
        }
        int64_t value{};
        int res = sscanf(m_cur_token->text.data, "%" SCNi64, &value);
        (void)res;
        assert(res > 0 && res != EOF && "numeric constant parsing failed");
        getToken();
        return value;
    }

    double parseFloat() {
        if (!check(t_float)) {
            return error("float constant expected"), 0;
        }
        double value{};
        int res = sscanf(m_cur_token->text.data, "%" SCNf64, &value);
        (void)res;
        assert(res > 0 && res != EOF && "numeric constant parsing failed");
        getToken();
        return value;
    }

    nkstr parseString(NkAllocator alloc) {
        auto const data = m_cur_token->text.data + 1;
        auto const len = m_cur_token->text.size - 2;
        getToken();

        auto const str = nk_alloc_t<char>(alloc, len + 1);
        memcpy(str, data, len);
        str[len] = '\0';

        return {str, len};
    }

    nkstr parseEspcaedString(NkAllocator alloc) {
        auto const data = m_cur_token->text.data + 1;
        auto const len = m_cur_token->text.size - 2;
        getToken();

        NkStringBuilder_T sb{};
        sb.alloc = alloc;
        nksb_str_unescape(&sb, {data, len});
        return {sb.data, sb.size};
    }

    struct ProcSignatureParseResult {
        NkArray<nkid> arg_names;
        NkArray<nktype_t> args_t;
        NkArray<nktype_t> ret_t;
        nkstr name{};
        bool is_variadic{};
        bool is_extern{};
        bool is_cdecl{};
    };

    ProcSignatureParseResult parseProcSignature() {
        ProcSignatureParseResult res{
            .arg_names = NkArray<nkid>::create(m_parse_alloc),
            .args_t = NkArray<nktype_t>::create(m_file_alloc),
            .ret_t = NkArray<nktype_t>::create(m_file_alloc),
        };
        EXPECT(t_proc);
        if (accept(t_extern)) {
            res.is_extern = true;
        } else if (accept(t_cdecl)) {
            res.is_cdecl = true;
        }
        DEFINE(name, parseId());
        res.name = name->text;
        EXPECT(t_par_l);
        do {
            if (check(t_par_r) || check(t_eof)) {
                break;
            }
            if (accept(t_period_3x)) {
                res.is_variadic = true;
                break;
            }
            nkid name = nkid_empty;
            if (!res.is_extern) {
                DEFINE(token, parseId());
                name = s2nkid(token->text);
                EXPECT(t_colon);
            }
            DEFINE(type, parseType());
            res.args_t.emplace(type);
            res.arg_names.emplace(name);
        } while (accept(t_comma));
        EXPECT(t_par_r);
        APPEND(res.ret_t, parseType());
        return res;
    }

    nktype_t parseType() {
        if (accept(t_void)) {
            return nkir_makeVoidType(m_types);
        }

        else if (accept(t_f32)) {
            return nkir_makeNumericType(m_types, Float32);
        } else if (accept(t_f64)) {
            return nkir_makeNumericType(m_types, Float64);
        } else if (accept(t_i16)) {
            return nkir_makeNumericType(m_types, Int16);
        } else if (accept(t_i32)) {
            return nkir_makeNumericType(m_types, Int32);
        } else if (accept(t_i64)) {
            return nkir_makeNumericType(m_types, Int64);
        } else if (accept(t_i8)) {
            return nkir_makeNumericType(m_types, Int8);
        } else if (accept(t_u16)) {
            return nkir_makeNumericType(m_types, Uint16);
        } else if (accept(t_u32)) {
            return nkir_makeNumericType(m_types, Uint32);
        } else if (accept(t_u64)) {
            return nkir_makeNumericType(m_types, Uint64);
        } else if (accept(t_u8)) {
            return nkir_makeNumericType(m_types, Uint8);
        }

        else if (check(t_id)) {
            DEFINE(token, parseId());
            auto name = s2nkid(token->text);
            auto found = m_decls.find(name);
            if (!found) {
                return error("undeclared identifier `%.*s`", (int)token->text.size, token->text.data), nullptr;
            } else if ((*found)->kind != Decl_Type) {
                return error("type expected"), nullptr;
            }
            return (*found)->as.type;
        }

        else if (accept(t_brace_l)) {
            auto types = NkArray<nktype_t>::create(m_tmp_alloc);
            auto counts = NkArray<size_t>::create(m_tmp_alloc);
            do {
                if (check(t_par_r) || check(t_eof)) {
                    break;
                }
                if (accept(t_bracket_l)) {
                    if (!check(t_int)) {
                        return error("integer constant expected"), nullptr;
                    }
                    APPEND(counts, (size_t)parseInt());
                    EXPECT(t_bracket_r);
                } else {
                    counts.emplace(1ull);
                }
                APPEND(types, parseType());
            } while (accept(t_comma));
            EXPECT(t_brace_r);
            return nkir_makeAggregateType(m_types, types.data(), counts.data(), types.size());
        }

        else if (accept(t_aster)) {
            DEFINE(target_type, parseType());
            return nkir_makePointerType(m_types, target_type);
        }

        else {
            return error("unexpected token `%.*s`", (int)m_cur_token->text.size, m_cur_token->text.data), nullptr;
        }
    }

    NkIrInstr parseInstr() {
        if (check(t_label)) {
            auto label = getLabel(s2nkid(m_cur_token->text));
            getToken();
            return nkir_make_label(label);
        }

        else if (accept(t_nop)) {
            return nkir_make_nop();
        } else if (accept(t_ret)) {
            return nkir_make_ret();
        }

        else if (accept(t_jmp)) {
            DEFINE(label, parseLabelRef());
            return nkir_make_jmp(label);
        } else if (accept(t_jmpz)) {
            DEFINE(cond, parseRef());
            EXPECT(t_comma);
            DEFINE(label, parseLabelRef());
            return nkir_make_jmpz(cond, label);
        } else if (accept(t_jmpnz)) {
            DEFINE(cond, parseRef());
            EXPECT(t_comma);
            DEFINE(label, parseLabelRef());
            return nkir_make_jmpnz(cond, label);
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

        else if (accept(t_neg)) {
            DEFINE(src, parseRef());
            EXPECT(t_minus_greater);
            DEFINE(dst, parseRef());
            return nkir_make_neg(dst, src);
        } else if (accept(t_mov)) {
            DEFINE(src, parseRef());
            EXPECT(t_minus_greater);
            DEFINE(dst, parseRef());
            return nkir_make_mov(dst, src);
        } else if (accept(t_lea)) {
            DEFINE(src, parseRef());
            EXPECT(t_minus_greater);
            DEFINE(dst, parseRef());
            return nkir_make_lea(dst, src);
        }

        else if (false) {
        }
#define BIN_IR(NAME)                                 \
    else if (accept(CAT(t_, NAME))) {                \
        DEFINE(lhs, parseRef());                     \
        EXPECT(t_comma);                             \
        DEFINE(rhs, parseRef());                     \
        EXPECT(t_minus_greater);                     \
        DEFINE(dst, parseRef());                     \
        return CAT(nkir_make_, NAME)(dst, lhs, rhs); \
    }
#include "nkb/ir.inl"

        else if (accept(t_cmp)) {
            if (false) {
            }
#define CMP_IR(NAME)                                     \
    else if (accept(CAT(t_, NAME))) {                    \
        DEFINE(lhs, parseRef());                         \
        EXPECT(t_comma);                                 \
        DEFINE(rhs, parseRef());                         \
        EXPECT(t_minus_greater);                         \
        DEFINE(dst, parseRef());                         \
        return CAT(nkir_make_cmp_, NAME)(dst, lhs, rhs); \
    }
#include "nkb/ir.inl"
            else {
                return error("unexpected token `%.*s`", (int)m_cur_token->text.size, m_cur_token->text.data),
                       NkIrInstr{};
            }
        }

        else {
            return error("unexpected token `%.*s`", (int)m_cur_token->text.size, m_cur_token->text.data), NkIrInstr{};
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
            auto const data = nkir_constRefDeref(m_ir, result_ref);

            switch (type->as.num.value_type) {
            case Int8:
                ASSIGN(*(int8_t *)data, parseInt());
                break;
            case Uint8:
                ASSIGN(*(uint8_t *)data, parseInt());
                break;
            case Int16:
                ASSIGN(*(int16_t *)data, parseInt());
                break;
            case Uint16:
                ASSIGN(*(uint16_t *)data, parseInt());
                break;
            case Int32:
                ASSIGN(*(int32_t *)data, parseInt());
                break;
            case Uint32:
                ASSIGN(*(uint32_t *)data, parseInt());
                break;
            case Int64:
                ASSIGN(*(int64_t *)data, parseInt());
                break;
            case Uint64:
                ASSIGN(*(uint64_t *)data, parseInt());
                break;
            case Float32:
                ASSIGN(*(float *)data, parseFloat());
                break;
            case Float64:
                ASSIGN(*(double *)data, parseFloat());
                break;
            }

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

    NkIrConst parseConst(nktype_t type) {
        auto data = nk_alloc(m_file_alloc, type->size);
        std::memset(data, 0, type->size);
        auto const cnst = nkir_makeConst(m_ir, data, type);
        CHECK(parseValue(nkir_makeRodataRef(m_ir, cnst), type));
        return cnst;
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
                return error("undeclared identifier `%.*s`", (int)token->text.size, token->text.data), NkIrRef{};
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
            case Decl_GlobalVar:
                result_ref = nkir_makeDataRef(m_ir, decl->as.global);
                break;
            case Decl_ExternData:
                result_ref = nkir_makeExternDataRef(m_ir, decl->as.extern_data);
                break;
            case Decl_ExternProc:
                result_ref = nkir_makeExternProcRef(m_ir, decl->as.extern_proc);
                break;
            case Decl_Type:
                return error("cannot reference a type"), NkIrRef{};
            case Decl_Const:
                result_ref = nkir_makeRodataRef(m_ir, decl->as.cnst);
                break;
            default:
                assert(!"unreachable");
                return {};
            }
        } else if (accept(t_ret)) {
            // TODO Support multiple return values
            result_ref = nkir_makeRetRef(m_ir, 0);
        }

        else if (check(t_string)) {
            auto const str = parseString(m_file_alloc);
            auto const str_t = nkir_makeArrayType(m_types, nkir_makeNumericType(m_types, Int8), str.size + 1);
            result_ref = nkir_makeRodataRef(m_ir, nkir_makeConst(m_ir, (void *)str.data, str_t));
        } else if (check(t_escaped_string)) {
            auto const str = parseEspcaedString(m_file_alloc);
            auto const str_t = nkir_makeArrayType(m_types, nkir_makeNumericType(m_types, Int8), str.size + 1);
            result_ref = nkir_makeRodataRef(m_ir, nkir_makeConst(m_ir, (void *)str.data, str_t));
        }

        else if (check(t_int)) {
            auto value = nk_alloc_t<int64_t>(m_file_alloc);
            ASSIGN(*value, parseInt());
            result_ref = nkir_makeRodataRef(m_ir, nkir_makeConst(m_ir, value, nkir_makeNumericType(m_types, Int64)));
        } else if (check(t_float)) {
            auto value = nk_alloc_t<double>(m_file_alloc);
            ASSIGN(*value, parseFloat());
            result_ref = nkir_makeRodataRef(m_ir, nkir_makeConst(m_ir, value, nkir_makeNumericType(m_types, Float64)));
        }

        else if (accept(t_colon)) {
            DEFINE(type, parseType());
            DEFINE(cnst, parseConst(type));
            result_ref = nkir_makeRodataRef(m_ir, cnst);
        }

        else {
            return error("unexpected token `%.*s`", (int)m_cur_token->text.size, m_cur_token->text.data), NkIrRef{};
        }

        if (accept(t_plus)) {
            DEFINE(value, parseInt());
            if (result_ref.indir) {
                result_ref.post_offset += value;
            } else {
                result_ref.offset += value;
            }
        }

        for (uint8_t i = 0; i < indir; i++) {
            EXPECT(t_bracket_r);
        }

        result_ref.indir += indir;

        if (accept(t_plus)) {
            DEFINE(value, parseInt());
            result_ref.post_offset += value;
        }

        if (accept(t_colon)) {
            ASSIGN(result_ref.type, parseType());
        }

        if (indir && deref) {
            return error("cannot have both dereference and address of"), NkIrRef{};
        }

        if (deref) {
            result_ref = nkir_makeAddressRef(m_ir, result_ref, nkir_makePointerType(m_types, result_ref.type));
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
            auto label = nkir_createLabel(m_ir, m_cur_token->text);
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
        found = m_decls.find(name);
        if (found) {
            return *found;
        }
        return nullptr;
    }

    NkIrRefArray parseRefArray() {
        auto refs = NkArray<NkIrRef>::create(m_tmp_alloc);
        EXPECT(t_par_l);
        do {
            if (check(t_par_r) || check(t_eof)) {
                break;
            }
            APPEND(refs, parseRef());
        } while (accept(t_comma));
        EXPECT(t_par_r);
        return {refs.data(), refs.size()};
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
            return error(
                "expected `%s` before `%.*s`", s_token_text[id], (int)m_cur_token->text.size, m_cur_token->text.data);
        }
    }

    NK_PRINTF_LIKE(2, 3) void error(char const *fmt, ...) {
        assert(!m_error_occurred && "Parser error already initialized");

        va_list ap;
        va_start(ap, fmt);
        NkStringBuilder_T sb{};
        sb.alloc = m_tmp_alloc;
        nksb_vprintf(&sb, fmt, ap);
        m_error_msg = {sb.data, sb.size};
        va_end(ap);

        m_error_occurred = true;
    }
};

} // namespace

void nkir_parse(
    NkIrParserState *parser,
    NkIrTypeCache *types,
    NkArena *file_arena,
    NkArena *tmp_arena,
    NkSlice<NkIrToken> tokens) {
    NK_LOG_TRC("%s", __func__);

    auto file_alloc = nk_arena_getAllocator(file_arena);

    parser->ir = nkir_createProgram(file_alloc);
    parser->entry_point.id = INVALID_ID;
    parser->error_msg = {};
    parser->ok = true;

    GeneratorState gen{
        .m_ir = parser->ir,
        .m_entry_point = parser->entry_point,
        .m_tokens = tokens,
        .m_types = types,
        .m_file_arena = file_arena,
        .m_tmp_arena = tmp_arena,
    };
    defer {
        nk_arena_free(&gen.m_parse_arena);
    };

    gen.generate();

    if (gen.m_error_occurred) {
        parser->error_msg = gen.m_error_msg;
        parser->ok = false;
        return;
    }
}
