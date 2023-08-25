#include "parser.hpp"

#include <cassert>

#include "nk/common/array.hpp"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nkb/common.h"

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
    NkAllocator m_alloc;
    NkAllocator m_tmp_alloc;

    nkstr m_error_msg{};
    bool m_error_occurred{};

    NkIrToken *m_cur_token{};

    Void generate() {
        assert(m_tokens.size() && m_tokens.back().id == t_eof && "ill-formed token stream");
        m_cur_token = &m_tokens[0];

        while (!check(t_eof)) {
            if (check(t_proc)) {
                parseProcSignature();
                EXPECT(t_brace_l);
                while (!check(t_brace_r)) {
                    // TODO Parse proc body
                    NK_LOG_WRN("Skipping " LOG_TOKEN(m_cur_token->id));
                    getToken();
                }
                EXPECT(t_brace_r);
            } else if (accept(t_extern)) {
                while (!check(t_newline)) {
                    // TODO Parse extern
                    NK_LOG_WRN("Skipping " LOG_TOKEN(m_cur_token->id));
                    getToken();
                }
            } else {
                return error("unexpected token `%.*s`", (int)m_cur_token->text.size, m_cur_token->text.data), Void{};
            }

            EXPECT(t_newline);
            while (accept(t_newline)) {
            }
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

    struct Field {
        nkid name;
        nktype_t type;
    };

    struct ProcSignatureParseResult {
        nkid name;
        NkArray<Field> params;
        nktype_t return_type;
    };

    ProcSignatureParseResult parseProcSignature(bool expect_names = true) {
        EXPECT(t_proc);
        accept(t_proc);
        DEFINE(token, parseId());
        auto name = s2nkid(token->text);
        NkArray<Field> params;
        params._alloc = m_tmp_alloc;
        EXPECT(t_par_l);
        do {
            if (check(t_par_r)) {
                break;
            }
            nkid name = nkid_empty;
            if (expect_names) {
                DEFINE(token, parseId());
                name = s2nkid(token->text);
            }
            DEFINE(type, parseType());
            *params.push() = {
                .name = name,
                .type = type,
            };
        } while (accept(t_comma));
        EXPECT(t_par_r);
        DEFINE(return_type, parseType());
        return {
            .name = name,
            .params = params,
            .return_type = return_type,
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

    nktype_t makeBasicType(NkIrBasicValueType value_type) {
        auto type = nk_alloc_t<NkIrType>(m_alloc);
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
        NkStringBuilder_T sb;
        nksb_init_alloc(&sb, m_tmp_alloc);
        defer {
            nksb_deinit(&sb);
        };
        nksb_vprintf(&sb, fmt, ap);
        m_error_msg = nk_strcpy(m_tmp_alloc, nksb_concat(&sb));
        va_end(ap);

        m_error_occurred = true;
    }
};

} // namespace

void nkir_parse(NkIrParserState *parser, NkAllocator alloc, NkAllocator tmp_alloc, NkSlice<NkIrToken> tokens) {
    NK_LOG_TRC("%s", __func__);

    parser->ir = nkir_createProgram(alloc);
    parser->error_msg = {};
    parser->ok = true;

    GeneratorState gen{
        .m_ir = parser->ir,
        .m_tokens = tokens,
        .m_alloc = alloc,
        .m_tmp_alloc = tmp_alloc,
    };

    gen.generate();

    if (gen.m_error_occurred) {
        parser->error_msg = gen.m_error_msg;
        parser->ok = false;
    }
}
