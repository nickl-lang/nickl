#include "parser.hpp"

#include <cassert>

#include "nk/common/logger.h"
#include "nk/common/string_builder.h"

namespace {

#define LOG_TOKEN(ID) "(%s, \"%s\")", s_token_id[ID], s_token_text[ID]

NK_LOG_USE_SCOPE(parser);

struct GeneratorState {
    NkSlice<NkIrToken> const m_tokens;
    NkAllocator m_tmp_alloc;

    nkstr m_error_msg{};
    bool m_error_occurred{};

    NkIrToken *m_cur_token{};

    void generate() {
        assert(m_tokens.size() && m_tokens.back().id == t_eof && "ill-formed token stream");
        m_cur_token = &m_tokens[0];
    }

private:
    void getToken() {
        assert(m_cur_token->id != t_eof);
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
        .m_tokens = tokens,
        .m_tmp_alloc = tmp_alloc,
    };

    gen.generate();

    if (gen.m_error_occurred) {
        parser->error_msg = gen.m_error_msg;
        parser->ok = false;
    }
}
