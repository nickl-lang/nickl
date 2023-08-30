#include "parser.hpp"

#include <cassert>
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
#define APPEND(AR, VAL) CHECK(*AR.push() = (VAL))
#define EXPECT(ID) CHECK(expect(ID))

struct GeneratorState {
    NkIrProg m_ir;
    NkSlice<NkIrToken> const m_tokens;

    NkArena *m_file_arena;
    NkArena *m_tmp_arena;
    NkArena m_parse_arena{};

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

        Decl_Proc,
        Decl_LocalVar,
        Decl_GlobalVar,
        Decl_ExternData,
        Decl_ExternProc,
    };

    struct Decl {
        union {
            ProcRecord proc;
            NkIrLocalVar local;
            NkIrExternProc extern_proc;
        } as;
        EDeclKind kind;
    };

    NkHashMap<nkid, Decl *> m_decls{};
    ProcRecord *m_cur_proc{};

    Void generate() {
        m_decls = decltype(m_decls)::create(nk_arena_getAllocator(&m_parse_arena));

        assert(m_tokens.size() && m_tokens.back().id == t_eof && "ill-formed token stream");
        m_cur_token = &m_tokens[0];

        while (!check(t_eof)) {
            if (check(t_proc)) {
                DEFINE(sig, parseProcSignature());

                auto proc = nkir_createProc(m_ir);
                nkir_startProc(
                    m_ir,
                    proc,
                    sig.name,
                    {
                        .args_t{sig.args_t.data(), sig.args_t.size()},
                        .ret_t{&sig.ret_t, 1},
                        .call_conv = NkCallConv_Nk,
                        .flags = 0,
                    });

                auto decl = new (nk_alloc_t<Decl>(nk_arena_getAllocator(&m_parse_arena))) Decl{
                    {.proc{
                        .proc = proc,
                        .locals = decltype(ProcRecord::locals)::create(nk_arena_getAllocator(&m_parse_arena)),
                        .labels = decltype(ProcRecord::labels)::create(nk_arena_getAllocator(&m_parse_arena)),
                    }},
                    Decl_Proc,
                };
                m_decls.insert(s2nkid(sig.name), decl);
                m_cur_proc = &decl->as.proc;

                EXPECT(t_brace_l);
                while (!check(t_brace_r) && !check(t_eof)) {
                    if (check(t_id)) {
                        DEFINE(token, parseId());
                        auto const name = s2nkid(token->text);
                        DEFINE(type, parseType());
                        auto decl = new (nk_alloc_t<Decl>(nk_arena_getAllocator(&m_parse_arena))) Decl{
                            {.local = nkir_makeLocalVar(m_ir, type)},
                            Decl_LocalVar,
                        };
                        m_cur_proc->locals.insert(name, decl);
                    } else {
                        DEFINE(instr, parseInstr());
                        nkir_gen(m_ir, {&instr, 1});
                    }
                    EXPECT(t_newline);
                    while (accept(t_newline)) {
                    }
                }
                EXPECT(t_brace_r);
            } else if (accept(t_extern)) {
                if (check(t_proc)) {
                    DEFINE(sig, parseProcSignature(false));

                    auto decl = new (nk_alloc_t<Decl>(nk_arena_getAllocator(&m_parse_arena))) Decl{
                        {.extern_proc = nkir_makeExternProc(
                             m_ir,
                             sig.name,
                             {
                                 .args_t{sig.args_t.data(), sig.args_t.size()},
                                 .ret_t{&sig.ret_t, 1},
                                 .call_conv = NkCallConv_Nk,
                                 .flags = 0,
                             })},
                        Decl_ExternProc,
                    };
                    m_decls.insert(s2nkid(sig.name), decl);
                } else {
                    return error("TODO extern kind not implemented"), Void{};
                }
            } else {
                return error("unexpected token `%.*s`", (int)m_cur_token->text.size, m_cur_token->text.data), Void{};
            }

            EXPECT(t_newline);
            while (accept(t_newline)) {
            }

            nk_arena_clear(m_tmp_arena);
        }

        return {};
    }

private:
    NkIrToken *parseId() {
        if (!check(t_id)) {
            return error("identifier expected"), nullptr;
        }
        NK_LOG_DBG("accept(id, \"%.*s\")", (int)m_cur_token->text.size, m_cur_token->text.data);
        auto id = m_cur_token;
        getToken();
        return id;
    }

    struct ProcSignatureParseResult {
        nkstr name;
        NkArray<nkid> arg_names;
        NkArray<nktype_t> args_t;
        nktype_t ret_t;
    };

    ProcSignatureParseResult parseProcSignature(bool expect_names = true) {
        EXPECT(t_proc);
        accept(t_proc);
        DEFINE(token, parseId());
        auto const name = token->text;
        auto args_t = NkArray<nktype_t>::create(nk_arena_getAllocator(m_tmp_arena));
        auto arg_names = NkArray<nkid>::create(nk_arena_getAllocator(m_tmp_arena));
        EXPECT(t_par_l);
        do {
            if (check(t_par_r) || check(t_eof)) {
                break;
            }
            nkid name = nkid_empty;
            if (expect_names) {
                DEFINE(token, parseId());
                name = s2nkid(token->text);
            }
            DEFINE(type, parseType());
            *args_t.push() = type;
            *arg_names.push() = name;
        } while (accept(t_comma));
        EXPECT(t_par_r);
        DEFINE(ret_t, parseType());
        return {
            .name = name,
            .arg_names = arg_names,
            .args_t = args_t,
            .ret_t = ret_t,
        };
    }

    nktype_t parseType() {
        EXPECT(t_colon);
        if (accept(t_f32)) {
            return makeBasicType(Float32);
        } else if (accept(t_f64)) {
            return makeBasicType(Float64);
        } else if (accept(t_i16)) {
            return makeBasicType(Int16);
        } else if (accept(t_i32)) {
            return makeBasicType(Int32);
        } else if (accept(t_i64)) {
            return makeBasicType(Int64);
        } else if (accept(t_i8)) {
            return makeBasicType(Int8);
        } else if (accept(t_u16)) {
            return makeBasicType(Uint16);
        } else if (accept(t_u32)) {
            return makeBasicType(Uint32);
        } else if (accept(t_u64)) {
            return makeBasicType(Uint64);
        } else if (accept(t_u8)) {
            return makeBasicType(Uint8);
        } else if (check(t_id)) {
            return error("TODO type identifiers not implemented"), nullptr;
        } else {
            return error("type expected"), nullptr;
        }
    }

    NkIrInstr parseInstr() {
        if (check(t_label)) {
            auto label = nkir_createLabel(m_ir);
            m_cur_proc->labels.insert(s2nkid(m_cur_token->text), label);
            getToken();
            return nkir_make_label(label);
        }

        else if (accept(t_call)) {
            NkIrRef dst{};
            DEFINE(proc, parseRef());
            EXPECT(t_comma);
            DEFINE(args, parseRefArray());
            return nkir_make_call(dst, proc, args);
        }

        return {};
    }

    NkIrRef parseRef() {
        if (check(t_id)) {
            DEFINE(token, parseId());
            auto const name = s2nkid(token->text);
            auto decl = resolve(name);
            if (!decl) {
                return error("undeclated identifier `%.*s`", (int)token->text.size, token->text.data), NkIrRef{};
            }
            switch (decl->kind) {
            case Decl_ExternProc:
                return nkir_makeExternProcRef(m_ir, decl->as.extern_proc);
            default:
                return error("TODO decl kind not handled"), NkIrRef{};
            }
        } else if (check(t_string)) {
            // TODO Implement string ref
            // auto cnst = nkir_makeConst(m_ir, nullptr, nullptr);
            // auto ref = nkir_makeRodataRef(m_ir, cnst);
            // ref.is_indirect = true;
            // return ref;
            getToken();
            return {};
        } else {
            return error("TODO ref not implemented"), NkIrRef{};
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
        auto refs = NkArray<NkIrRef>::create(nk_arena_getAllocator(m_tmp_arena));
        EXPECT(t_brace_l);
        do {
            if (check(t_brace_r) || check(t_eof)) {
                break;
            }
            APPEND(refs, parseRef());
        } while (accept(t_comma));
        EXPECT(t_brace_r);
        return {refs.data(), refs.size()};
    }

    nktype_t makeBasicType(NkIrBasicValueType value_type) {
        auto type = nk_alloc_t<NkIrType>(nk_arena_getAllocator(m_file_arena));
        *type = {
            .as{
                .basic{
                    .value_type = value_type,
                },
            },
            .size = (uint8_t)NKIR_BASIC_TYPE_SIZE(value_type),
            .align = (uint8_t)NKIR_BASIC_TYPE_SIZE(value_type),
            .kind = NkType_Basic,
        };
        return type;
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
        nksb_init_alloc(&sb, nk_arena_getAllocator(m_tmp_arena));
        nksb_vprintf(&sb, fmt, ap);
        m_error_msg = nksb_concat(&sb);
        va_end(ap);

        m_error_occurred = true;
    }
};

} // namespace

void nkir_parse(NkIrParserState *parser, NkArena *file_arena, NkArena *tmp_arena, NkSlice<NkIrToken> tokens) {
    NK_LOG_TRC("%s", __func__);

    parser->ir = nkir_createProgram(nk_arena_getAllocator(file_arena));
    parser->error_msg = {};
    parser->ok = true;

    GeneratorState gen{
        .m_ir = parser->ir,
        .m_tokens = tokens,

        .m_file_arena = file_arena,
        .m_tmp_arena = tmp_arena,
    };

    gen.generate();

    nk_arena_free(&gen.m_parse_arena);

    if (gen.m_error_occurred) {
        parser->error_msg = gen.m_error_msg;
        parser->ok = false;
    }
}
