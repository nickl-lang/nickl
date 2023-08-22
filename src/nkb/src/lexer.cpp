#include "lexer.h"

#include <cctype>

#include "nk/common/logger.h"

namespace {

NK_LOG_USE_SCOPE(lexer);

struct LexerState {
    nkstr const m_src;
    NkAllocator m_alloc;

    size_t m_pos = 0;
    size_t m_lin = 1;
    size_t m_col = 1;

    NkIrToken m_token{};

    void scan() {
        skipSpaces();

        if (m_pos == 0 && on('#') && on('!', 1)) {
            while (chr() && !on('\n')) {
                advance();
            }
            skipSpaces();
        }

        while ((on('/') && on('/', 1)) || (on('/') && on('*', 1))) {
            if (on('/', 1)) {
                while (!on('\n')) {
                    advance();
                }
            } else {
                advance(2);
                while (chr()) {
                    if (on('*') && on('/', 1)) {
                        advance(2);
                        break;
                    } else {
                        advance();
                    }
                }
            }
            skipSpaces();
        }

        m_token = {
            .text = {m_src.data + m_pos, 0},
            .id = /*t_empty*/ 0,
            .lin = m_lin,
            .col = m_col,
        };

        if (!chr()) {
            m_token.id = /*t_empty*/ 1;
        } else {
            accept();
        }
    }

private:
    char chr(int64_t offset = 0) {
        return m_pos + offset < m_src.size ? m_src.data[m_pos + offset] : '\0';
    }

    template <int (*F)(int)>
    int chk(int64_t offset = 0) {
        return F(chr(offset));
    }

    bool on(char c, int64_t offset = 0) {
        return chr(offset) == c;
    }

    bool onAlphaOrUscr(int64_t offset = 0) {
        return chk<std::isalpha>(offset) || on('_', offset);
    }

    bool onAlnumOrUscr(int64_t offset = 0) {
        return chk<std::isalnum>(offset) || on('_', offset);
    }

    void advance(int64_t n = 1) {
        for (int64_t i = 0; i < n && chr(); i++) {
            m_pos++;
            if (on('\n')) {
                m_col = 0;
                m_lin++;
            } else {
                m_col++;
            }
        }
    }

    void skipSpaces() {
        while (chk<std::isspace>()) {
            advance();
        }
    }

    void accept(int64_t n = 1) {
        advance(n);
        m_token.text.size += n;
    }
};

} // namespace

NkIrLexerResult nkir_lex(NkAllocator alloc, nkstr src) {
    NK_LOG_TRC("%s", __func__);

    NkIrLexerResult res{
        .tokens{},
        .error_msg{},
        .ok = true,
    };

    LexerState lexer{
        .m_src = src,
        .m_alloc = alloc,
    };

    do {
        lexer.scan();
        // tokens.emplace_back(engine.m_token);
        NK_LOG_DBG(
            "%s: \"%.*s\"",
            "ID" /*s_token_id[tokens.back().id]*/,
            (int)lexer.m_token.text.size,
            lexer.m_token.text.data);
        // if (engine.m_token.id == t_error) {
        //     return false;
        // }
    } while (lexer.m_token.id != 1 /*t_eof*/);

    return res;
}
