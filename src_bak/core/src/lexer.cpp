#include "nkl/core/lexer.hpp"

#include <cctype>

#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"

namespace nkl {

namespace {

LOG_USE_SCOPE(nkl::lexer);

char const *s_operators[] = {
#define OP(id, text) text,
#include "nkl/core/tokens.inl"
};

char const *s_keywords[] = {
#define KW(id) #id,
#include "nkl/core/tokens.inl"
};

struct ScanEngine {
    string const m_text;

    string &m_err;

    size_t m_pos = 0;
    size_t m_lin = 1;
    size_t m_col = 1;

    Token m_token{};

    void scan() {
        EASY_FUNCTION(::profiler::colors::Amber200);
        LOG_TRC(__FUNCTION__);

        skipSpaces();

        while (on('/') && on('/', 1)) {
            while (chr() && !on('\n')) {
                advance();
            }
            skipSpaces();
        }

        m_token = {
            .text = {m_text.data + m_pos, 0},
            .pos = m_pos,
            .lin = m_lin,
            .col = m_col,
            .id = t_empty,
        };

        if (!chr()) {
            m_token.id = t_eof;
        } else if (on('"')) {
            advance();
            m_token.text.data++;

            bool escaped = false;

            while (chr() && !on('\"') && !on('\n')) {
                accept();
                if (on('\\', -1)) {
                    switch (chr()) {
                    case 'n':
                    case 't':
                    case '0':
                    case '\\':
                    case '"':
                    case '\n':
                        escaped = true;
                        accept();
                        break;
                    default:
                        if (!chr()) {
                            return error("unexpected end of file");
                        } else {
                            return error("invalid escape sequence '\\%c'", chr());
                        }
                    }
                }
            }

            if (!on('\"')) {
                return error("invalid string constant");
            }

            m_token.id = escaped ? t_escaped_str_const : t_str_const;

            advance();
        } else if (chk<std::isdigit>()) {
            m_token.id = t_int_const;

            if (on('0') && on('x', 1)) {
                accept(2);
                if (!chk<std::isxdigit>()) {
                    return error("invalid hex literal");
                }
                while (chk<std::isxdigit>()) {
                    accept();
                }
            } else {
                while (chk<std::isdigit>()) {
                    accept();
                }

                if (on('.')) {
                    m_token.id = t_float_const;
                    do {
                        accept();
                    } while (chk<std::isdigit>());
                }

                if (chk<std::tolower>() == 'e') {
                    m_token.id = t_float_const;
                    accept();
                    if (on('-') || on('+')) {
                        accept();
                    }
                    if (!chk<std::isdigit>()) {
                        return error("invalid float literal");
                    }
                    while (chk<std::isdigit>()) {
                        accept();
                    }
                }
            }

            if (chk<std::isalpha>()) {
                return error("invalid suffix");
            }
        } else if (onAlphaOrUscr()) {
            while (onAlnumOrUscr()) {
                accept();
            }

            auto it = std::begin(s_keywords) + 1;
            for (; it != std::end(s_keywords) &&
                   (m_token.text.size != std::strlen(*it) || *it != std_view(m_token.text));
                 ++it) {
            }

            if (it != std::end(s_keywords)) {
                size_t kw_index = std::distance(std::begin(s_keywords), it);
                m_token.id = (ETokenID)(t_keyword_marker + kw_index);
            } else {
                m_token.id = t_id;
            }
        } else {
            size_t op_index = 0;
            size_t op_len = 0;

            for (auto it = std::begin(s_operators); it != std::end(s_operators); ++it) {
                const size_t len = std::strlen(*it);
                size_t i = 0;
                for (; i < len && !on('\0', i) && on((*it)[i], i); i++) {
                }
                if (i == len) {
                    op_index = std::distance(std::begin(s_operators), it);
                    op_len = len;
                }
            }

            if (op_index > 0) {
                accept(op_len);
                m_token.id = (ETokenID)(t_operator_marker + op_index);
            } else {
                return error("unknown token '%.*s'", m_token.text.size, m_token.text.data);
            }
        }
    }

private:
    char chr(int64_t offset = 0) {
        return m_pos + offset < m_text.size ? m_text[m_pos + offset] : '\0';
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

    void error(char const *fmt, ...) {
        if (chr()) {
            accept();
        }
        va_list ap;
        va_start(ap, fmt);
        LibcAllocator allocator;
        m_err = string_vformat(allocator, fmt, ap);
        va_end(ap);
        m_token.id = t_error;
    }
};

} // namespace

bool Lexer::lex(string text) {
    EASY_FUNCTION(::profiler::colors::Amber200);
    LOG_TRC(__FUNCTION__);

    tokens.clear();
    err = {};

    ScanEngine engine{text, err};

    do {
        engine.scan();
        tokens.push() = engine.m_token;
        LOG_DBG(
            "%s: \"%.*s\"",
            s_token_id[tokens.back().id],
            tokens.back().text.size,
            tokens.back().text.data);
        if (engine.m_token.id == t_error) {
            return false;
        }
    } while (tokens.back().id != t_eof);

    return true;
}

} // namespace nkl
