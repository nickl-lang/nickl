#include "lexer.h"

#include <cctype>
#include <cinttypes>
#include <cstdarg>
#include <cstring>
#include <iterator>

#include "nkl/common/token.h"
#include "ntk/allocator.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

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

struct ScannerState {
    NkString const m_text;
    NkArena *m_tmp_arena;

    u32 m_pos = 0;
    u32 m_lin = 1;
    u32 m_col = 1;

    NklToken m_token{};
    NkString m_error_msg{};

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
            .id = t_empty,
            .pos = m_pos,
            .len = 0,
            .lin = m_lin,
            .col = m_col,
        };

        if (!chr()) {
            m_token.id = t_eof;
        } else if (on('\n')) {
            while (on('\n')) {
                accept();
            }
            m_token.id = t_newline;
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
                                m_token.pos = m_pos - 1;
                                m_token.len = 2;
                                m_token.lin = m_lin;
                                m_token.col = m_col - 1;
                                if (isprint(chr())) {
                                    return error("invalid escape sequence `\\%c`", chr());
                                } else {
                                    return error("invalid escape sequence `\\\\x%" PRIx8 "`", chr() & 0xff);
                                }
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
        } else if (chk<isdigit>(on('-') ? 1 : 0)) {
            if (on('-')) {
                accept();
            }

            m_token.id = t_int;

            while (chk<isdigit>()) {
                accept();
            }

            if (on('.')) {
                m_token.id = t_float;
                do {
                    accept();
                } while (chk<isdigit>());
            }

            if (chk<tolower>() == 'e') {
                m_token.id = t_float;
                accept();
                if (on('-') || on('+')) {
                    accept();
                }
                if (!chk<isdigit>()) {
                    return error("invalid float literal");
                }
                while (chk<isdigit>()) {
                    accept();
                }
            }

            if (chk<isalpha>()) {
                accept();
                return error("invalid suffix");
            }
        } else if (onAlphaOrUscr()) {
            while (onAlnumOrUscr()) {
                accept();
            }

            auto const token_str = nk_s2stdView(nkl_getTokenStr(&m_token, m_text));
            auto it = std::begin(s_keywords) + 1;
            for (; it != std::end(s_keywords) && (m_token.len != strlen(*it) || *it != token_str); ++it) {
            }

            if (it != std::end(s_keywords)) {
                usize kw_index = std::distance(std::begin(s_keywords), it);
                m_token.id = (ENkIrTokenId)(t_keyword_marker + kw_index);
            } else {
                m_token.id = t_id;
            }
        } else if (on('@')) {
            accept();

            if (!onAlphaOrUscr()) {
                error("invalid label");
            }
            while (onAlnumOrUscr()) {
                accept();
            }

            m_token.id = t_label;
        } else {
            usize op_index = 0;
            usize op_len = 0;

            for (auto it = std::begin(s_operators); it != std::end(s_operators); ++it) {
                const usize len = strlen(*it);
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
                m_token.id = (ENkIrTokenId)(t_operator_marker + op_index);
            } else {
                accept();
                if (isprint(chr(-1))) {
                    return error("unexpected character `%c`", chr(-1));
                } else {
                    return error("unexpected byte `\\x%" PRIx8 "`", chr(-1) & 0xff);
                }
            }
        }
    }

private:
    char chr(i64 offset = 0) {
        return m_pos + offset < (u32)m_text.size ? m_text.data[m_pos + offset] : '\0';
    }

    template <int (*F)(int)>
    int chk(i64 offset = 0) {
        return F(chr(offset));
    }

    bool on(char c, i64 offset = 0) {
        return chr(offset) == c;
    }

    bool onAlphaOrUscr(i64 offset = 0) {
        return chk<isalpha>(offset) || on('_', offset);
    }

    bool onAlnumOrUscr(i64 offset = 0) {
        return chk<isalnum>(offset) || on('_', offset);
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
        while (chk<isspace>() && !on('\n')) {
            advance();
        }
    }

    void accept(i64 n = 1) {
        advance(n);
        m_token.len += n;
    }

    NK_PRINTF_LIKE(2, 3) void error(char const *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        NkStringBuilder sb{};
        sb.alloc = nk_arena_getAllocator(m_tmp_arena);
        nksb_vprintf(&sb, fmt, ap);
        m_error_msg = {NK_SLICE_INIT(sb)};
        va_end(ap);
        m_token.id = t_error;
    }
};

} // namespace

void nkir_lex(NkIrLexerState *lexer, NkArena *file_arena, NkArena *tmp_arena, NkString text) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    lexer->tokens = {0, 0, 0, nk_arena_getAllocator(file_arena)};
    nkda_reserve(&lexer->tokens, 1000);

    lexer->error_msg = {};
    lexer->ok = true;

    ScannerState scanner{
        .m_text = text,
        .m_tmp_arena = tmp_arena,
    };

    do {
        scanner.scan();
        nkda_append(&lexer->tokens, scanner.m_token);

        if (scanner.m_token.id == t_error) {
            lexer->ok = false;
            lexer->error_msg = scanner.m_error_msg;
            break;
        }

#ifdef ENABLE_LOGGING
        NKSB_FIXED_BUFFER(sb, 256);
        nks_escape(nksb_getStream(&sb), nkl_getTokenStr(&scanner.m_token, text));
        NK_LOG_DBG("%s: \"" NKS_FMT "\"", s_token_id[scanner.m_token.id], NKS_ARG(sb));
#endif // ENABLE_LOGGING
    } while (scanner.m_token.id != t_eof);
}
