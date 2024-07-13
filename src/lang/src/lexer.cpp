#include "lexer.hpp"

#include <cctype>
#include <cstdarg>
#include <cstring>

#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "token.hpp"

const char *s_token_id[] = {
#define OP(ID, TEXT) #ID,
#define KW(ID) #ID,
#define SP(ID, TEXT) #ID,
#include "tokens.inl"
};

const char *s_token_text[] = {
#define OP(ID, TEXT) TEXT,
#define KW(ID) #ID,
#define SP(ID, TEXT) TEXT,
#include "tokens.inl"
};

namespace {

NK_LOG_USE_SCOPE(lexer);

char const *s_operators[] = {
#define OP(id, text) text,
#include "tokens.inl"
};

char const *s_keywords[] = {
#define KW(id) #id,
#include "tokens.inl"
};

struct ScanEngine {
    NkString const m_src;
    std::string &m_err_str;

    usize m_pos = 0;
    usize m_lin = 1;
    usize m_col = 1;

    NklToken m_token{};

    void scan() {
        NK_PROF_FUNC();

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
            .pos = m_pos,
            .lin = m_lin,
            .col = m_col,
            .id = t_empty,
        };

        if (!chr()) {
            m_token.id = t_eof;
        } else if (on('"')) {
            accept();

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
                                m_token.text.data = m_src.data + m_pos - 1;
                                m_token.text.size = 2;
                                m_token.pos = m_pos - 1;
                                m_token.lin = m_lin;
                                m_token.col = m_col - 1;
                                return error("invalid escape sequence `\\%c`", chr());
                            }
                    }
                }
            }

            if (!on('\"')) {
                accept();
                return error("invalid string constant");
            }

            m_token.id = escaped ? t_escaped_string : t_string;

            accept();
        } else if (chk<std::isdigit>()) {
            if (on('0') && on('x', 1)) {
                m_token.id = t_int_hex;
                accept(2);
                bool invalid_num = false;
                if (!onAlnumOrUscr()) {
                    invalid_num = true;
                } else {
                    while (onAlnumOrUscr()) {
                        if (on('_')) {
                            m_token.id = t_int_hex_uscore;
                        } else if (!chk<std::isxdigit>()) {
                            invalid_num = true;
                        }
                        accept();
                    }
                }
                if (invalid_num) {
                    return error("invalid hex literal");
                }
            } else {
                m_token.id = t_int;

                while (chk<std::isdigit>() || on('_')) {
                    if (on('_')) {
                        m_token.id = t_int_uscore;
                    }
                    accept();
                }

                if (on('.')) {
                    m_token.id = t_float;
                    do {
                        accept();
                    } while (chk<std::isdigit>());
                }

                if (chk<std::tolower>() == 'e') {
                    m_token.id = t_float;
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
                accept();
                return error("invalid suffix");
            }
        } else if (onAlphaOrUscr()) {
            while (onAlnumOrUscr()) {
                accept();
            }

            auto it = std::begin(s_keywords) + 1;
            for (; it != std::end(s_keywords) &&
                   (m_token.text.size != std::strlen(*it) || *it != nk_s2stdView(m_token.text));
                 ++it) {
            }

            if (it != std::end(s_keywords)) {
                usize kw_index = std::distance(std::begin(s_keywords), it);
                m_token.id = (ETokenId)(t_keyword_marker + kw_index);
            } else {
                m_token.id = t_id;
            }
        } else {
            usize op_index = 0;
            usize op_len = 0;

            for (auto it = std::begin(s_operators); it != std::end(s_operators); ++it) {
                const usize len = std::strlen(*it);
                usize i = 0;
                for (; i < len && !on('\0', i) && on((*it)[i], i); i++) {
                }
                if (i == len) {
                    op_index = std::distance(std::begin(s_operators), it);
                    op_len = len;
                }
            }

            if (op_index > 0) {
                accept(op_len);
                m_token.id = (ETokenId)(t_operator_marker + op_index);

                if (m_token.id == t_at && onAlphaOrUscr(1)) {
                    while (onAlnumOrUscr()) {
                        accept();
                    }

                    m_token.id = t_intrinsic;
                } else if (m_token.id == t_number && onAlphaOrUscr(1)) {
                    while (onAlnumOrUscr()) {
                        accept();
                    }

                    m_token.id = t_tag;
                }
            } else {
                accept();
                return error("unknown token `%c`", chr(-1));
            }
        }
    }

private:
    char chr(i64 offset = 0) {
        return m_pos + offset < m_src.size ? m_src.data[m_pos + offset] : '\0';
    }

    template <int (*F)(int)>
    int chk(i64 offset = 0) {
        return F(chr(offset));
    }

    bool on(char c, i64 offset = 0) {
        return chr(offset) == c;
    }

    bool onAlphaOrUscr(i64 offset = 0) {
        return chk<std::isalpha>(offset) || on('_', offset);
    }

    bool onAlnumOrUscr(i64 offset = 0) {
        return chk<std::isalnum>(offset) || on('_', offset);
    }

    void advance(i64 n = 1) {
        for (i64 i = 0; i < n && chr(); i++) {
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

    void accept(i64 n = 1) {
        advance(n);
        m_token.text.size += n;
    }

    NK_PRINTF_LIKE(2) void error(char const *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        NkStringBuilder sb{};
        nksb_vprintf(&sb, fmt, ap);
        m_err_str = nk_s2stdStr({NKS_INIT(sb)});
        nksb_free(&sb);
        va_end(ap);
        m_token.id = t_error;
    }
};

} // namespace

bool nkl_lex(NkString src, std::vector<NklToken> &tokens, std::string &err_str) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    ScanEngine engine{src, err_str};

    do {
        engine.scan();
        tokens.emplace_back(engine.m_token);
        NK_LOG_DBG("%s: \"" NKS_FMT "\"", s_token_id[tokens.back().id], NKS_ARG(tokens.back().text));
        if (engine.m_token.id == t_error) {
            return false;
        }
    } while (tokens.back().id != t_eof);

    return true;
}
