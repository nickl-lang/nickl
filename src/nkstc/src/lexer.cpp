#include "lexer.h"

#include <cctype>
#include <cstdarg>
#include <cstring>
#include <iterator>

#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "ntk/allocator.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

const char *s_nkst_token_id[] = {
#define OP(ID, TEXT) #ID,
#define SP(ID, TEXT) #ID,
#include "tokens.inl"
};

const char *s_nkst_token_text[] = {
#define OP(ID, TEXT) TEXT,
#define SP(ID, TEXT) TEXT,
#include "tokens.inl"
};

namespace {

NK_LOG_USE_SCOPE(lexer);

char const *s_operators[] = {
#define OP(id, text) text,
#include "tokens.inl"
};

struct ScannerState {
    NkAtom const m_file;
    NkString const m_text;

    u32 m_pos = 0;
    u32 m_lin = 1;
    u32 m_col = 1;

    NklToken m_token{};

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
            .id = t_empty,
            .pos = m_pos,
            .len = 0,
            .lin = m_lin,
            .col = m_col,
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
            m_token.id = t_id;
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
                m_token.id = (NkStTokenId)(t_operator_marker + op_index);
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
            if (on('\n')) {
                m_col = 1;
                m_lin++;
            } else {
                m_col++;
            }
            m_pos++;
        }
    }

    void skipSpaces() {
        while (chk<isspace>()) {
            advance();
        }
    }

    void accept(i64 n = 1) {
        advance(n);
        m_token.len += n;
    }

    NK_PRINTF_LIKE(2) void error(char const *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        // TODO: Leaking error token, need to use scatch arena
        auto token = nk_allocT<NklToken>(nk_default_allocator);
        *token = m_token;
        nkl_vreportError(m_file, token, fmt, ap);
        va_end(ap);
    }
};

} // namespace

NklTokenArray nkst_lex(NkAllocator alloc, NkAtom file, NkString text) {
    NK_LOG_TRC("%s", __func__);

    NklTokenDynArray tokens{NKDA_INIT(alloc)};
    nkda_reserve(&tokens, 1000);

    ScannerState scanner{
        .m_file = file,
        .m_text = text,
    };

    do {
        scanner.scan();
        nkda_append(&tokens, scanner.m_token);

        if (nkl_getErrorCount()) {
            return {};
        }

#ifdef ENABLE_LOGGING
        NKSB_FIXED_BUFFER(sb, 256);
        nks_escape(nksb_getStream(&sb), nkl_getTokenStr(&scanner.m_token, text));
        NK_LOG_DBG("%s: \"" NKS_FMT "\"", s_nkst_token_id[scanner.m_token.id], NKS_ARG(sb));
#endif // ENABLE_LOGGING
    } while (scanner.m_token.id != t_eof);

    return {NKS_INIT(tokens)};
}
