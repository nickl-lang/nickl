#include "nkl/core/lexer.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>
// #include <cstdarg>
// #include <iterator>

#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "ntk/allocator.h"
#include "ntk/arena.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

// const char *s_nkst_token_id[] = {
//     #define OP(ID, TEXT) #ID,
//     #define SP(ID, TEXT) #ID,
//     #include "tokens.inl"
// };

// const char *s_nkst_token_text[] = {
//     #define OP(ID, TEXT) TEXT,
//     #define SP(ID, TEXT) TEXT,
//     #include "tokens.inl"
// };

// char const *s_operators[] = {
//     #define OP(id, text) text,
//     #include "tokens.inl"
// };

NK_LOG_USE_SCOPE(lexer);

// struct ScannerState {
//     NkAtom const m_file;
//     NkString const m_text;

//     u32 m_pos = 0;
//     u32 m_lin = 1;
//     u32 m_col = 1;

//     NklToken m_token{};

//     void scan() {
//     }

// private:
//     NK_PRINTF_LIKE(2) void error(char const *fmt, ...) {
//         va_list ap;
//         va_start(ap, fmt);
//         // TODO: Leaking error token, need to use scatch arena
//         auto token = nk_allocT<NklToken>(nk_default_allocator);
//         *token = m_token;
//         nkl_vreportError(m_file, token, fmt, ap);
//         va_end(ap);
//     }
// };

// } // namespace

typedef struct {
    NkString const text;

    char const **const keywords;
    char const **const operators;
    char const *const tag_prefixes;
    u32 const keywords_base;
    u32 const operators_base;
    u32 const tags_base;

    u32 pos;
    u32 lin;
    u32 col;

    NklToken token;
} LexerState;

static char chr(LexerState const *lex, i64 offset) {
    return lex->pos + offset < (u32)lex->text.size ? lex->text.data[lex->pos + offset] : '\0';
}

static bool on(LexerState const *lex, char c, i64 offset) {
    return chr(lex, offset) == c;
}

static int onSpace(LexerState const *lex, i64 offset) {
    return isspace(chr(lex, offset));
}

static int onAlpha(LexerState const *lex, i64 offset) {
    return isalpha(chr(lex, offset));
}

static int onAlnum(LexerState const *lex, i64 offset) {
    return isalnum(chr(lex, offset));
}

static int onDigit(LexerState const *lex, i64 offset) {
    return isdigit(chr(lex, offset));
}

static int onLower(LexerState const *lex, i64 offset) {
    return tolower(chr(lex, offset));
}

static bool onAlphaOrUscr(LexerState const *lex, i64 offset) {
    return onAlpha(lex, offset) || on(lex, '_', offset);
}

static bool onAlnumOrUscr(LexerState const *lex, i64 offset) {
    return onAlnum(lex, offset) || on(lex, '_', offset);
}

static void advance(LexerState *lex, i64 n) {
    for (i64 i = 0; i < n && chr(lex, 0); i++) {
        if (on(lex, '\n', 0)) {
            lex->col = 1;
            lex->lin++;
        } else {
            lex->col++;
        }
        lex->pos++;
    }
}

static void accept(LexerState *lex, i64 n) {
    advance(lex, n);
    lex->token.len += n;
}

static void discard(LexerState *lex) {
    lex->pos -= lex->token.len;
    lex->col -= lex->token.len;
    lex->token.len = 0;
}

static void skipSpaces(LexerState *lex) {
    while (onSpace(lex, 0)) {
        advance(lex, 1);
    }
}

static bool scan(LexerState *lex) {
    skipSpaces(lex);

    if (lex->pos == 0 && on(lex, '#', 0) && on(lex, '!', 1)) {
        while (chr(lex, 0) && !on(lex, '\n', 0)) {
            advance(lex, 1);
        }
        skipSpaces(lex);
    }

    while ((on(lex, '/', 0) && on(lex, '/', 1)) || (on(lex, '/', 0) && on(lex, '*', 1))) {
        if (on(lex, '/', 1)) {
            while (!on(lex, '\n', 0)) {
                advance(lex, 1);
            }
        } else {
            advance(lex, 2);
            while (chr(lex, 0)) {
                if (on(lex, '*', 0) && on(lex, '/', 1)) {
                    advance(lex, 2);
                    break;
                } else {
                    advance(lex, 1);
                }
            }
        }
        skipSpaces(lex);
    }

    lex->token = (NklToken){
        .id = NklBaseToken_Empty,
        .pos = lex->pos,
        .len = 0,
        .lin = lex->lin,
        .col = lex->col,
    };

    if (!chr(lex, 0)) {
        lex->token.id = NklBaseToken_Eof;
        return true;
    }

    // if (on(lex, '"', 0)) {
    //     accept();

    //     bool escaped = false;

    //     while (chr() && !on('\"') && !on('\n')) {
    //         accept();
    //         if (on('\\', -1)) {
    //             switch (chr()) {
    //                 case 'n':
    //                 case 't':
    //                 case '0':
    //                 case '\\':
    //                 case '"':
    //                 case '\n':
    //                     escaped = true;
    //                     accept();
    //                     break;
    //                 default:
    //                     if (!chr()) {
    //                         return error("unexpected end of file");
    //                     } else {
    //                         lex->token.pos = lex->pos - 1;
    //                         lex->token.len = 2;
    //                         lex->token.lin = lex->lin;
    //                         lex->token.col = lex->col - 1;
    //                         if (isprint(chr())) {
    //                             return error("invalid escape sequence `\\%c`", chr());
    //                         } else {
    //                             return error("invalid escape sequence `\\\\x%" PRIx8 "`", chr() & 0xff);
    //                         }
    //                     }
    //             }
    //         }
    //     }

    //     if (!on('\"')) {
    //         accept();
    //         return error("invalid string constant");
    //     }

    //     lex->token.id = escaped ? t_escaped_string : t_string;

    //     accept();
    //     return true;
    // }

    if (onDigit(lex, on(lex, '-', 0) ? 1 : 0)) {
        if (on(lex, '-', 0)) {
            accept(lex, 1);
        }

        lex->token.id = NklBaseToken_Int;

        while (onDigit(lex, 0)) {
            accept(lex, 1);
        }

        if (on(lex, '.', 0)) {
            lex->token.id = NklBaseToken_Float;
            do {
                accept(lex, 1);
            } while (onDigit(lex, 0));
        }

        if (onLower(lex, 0) == 'e') {
            lex->token.id = NklBaseToken_Float;
            accept(lex, 1);
            if (on(lex, '-', 0) || on(lex, '+', 0)) {
                accept(lex, 1);
            }
            if (!onDigit(lex, 0)) {
                NK_LOG_ERR("invalid float literal");
                // return error("invalid float literal");
                return false;
            }
            while (onDigit(lex, 0)) {
                accept(lex, 1);
            }
        }

        if (onAlpha(lex, 0)) {
            accept(lex, 1);
            NK_LOG_ERR("invalid suffix");
            // return error("invalid suffix");
            return false;
        }

        return true;
    }

    if (onAlphaOrUscr(lex, 0)) {
        while (onAlnumOrUscr(lex, 0)) {
            accept(lex, 1);
        }

        NkString const token_str = nkl_getTokenStr(&lex->token, lex->text);
        char const **it = lex->keywords;
        for (; *it && !nks_equalCStr(token_str, *it); it++) {
        }

        if (*it) {
            ptrdiff_t const idx = it - lex->keywords;
            lex->token.id = lex->keywords_base + idx;
        } else {
            lex->token.id = NklBaseToken_Id;
        }

        return true;
    }

    {
        char const *it = lex->tag_prefixes;
        for (; *it && !(on(lex, *it, 0) && onAlphaOrUscr(lex, 1)); it++) {
        }

        if (*it) {
            do {
                accept(lex, 1);
            } while (onAlnumOrUscr(lex, 0));

            ptrdiff_t const idx = it - lex->tag_prefixes;
            lex->token.id = lex->tags_base + idx;
            return true;
        }
    }

    {
        char const **it = lex->operators;
        for (; *it; it++) {
            char const *op = *it;

            while (*op && on(lex, *op, 0)) {
                accept(lex, 1);
                op++;
            }

            if (*op) {
                discard(lex);
            } else {
                break;
            }
        }

        if (*it) {
            ptrdiff_t const idx = it - lex->operators;
            lex->token.id = lex->operators_base + idx;
            return true;
        }
    }

    if (isprint(chr(lex, 0))) {
        NK_LOG_ERR("unexpected character `%c`", chr(lex, 0));
        // return error("unexpected character `%c`", chr(lex, 0));
        return false;
    } else {
        NK_LOG_ERR("unexpected byte `\\x%" PRIx8 "`", chr(lex, 0) & 0xff);
        // return error("unexpected byte `\\x%" PRIx8 "`", chr(lex, 0) & 0xff);
        return false;
    }
}

NklTokenArray nkl_lex(NklLexerData const *data, NkArena *arena, NkString text) {
    NK_LOG_TRC("%s", __func__);

    NklTokenDynArray tokens = {NKDA_INIT(nk_arena_getAllocator(arena))};
    nkda_reserve(&tokens, 1000);

    LexerState lex = {
        .text = text,

        .keywords = data->keywords,
        .operators = data->operators,
        .tag_prefixes = data->tag_prefixes,
        .keywords_base = data->keywords_base,
        .operators_base = data->operators_base,
        .tags_base = data->tags_base,

        .pos = 0,
        .lin = 1,
        .col = 1,

        .token = {0},
    };

    do {
        if (!scan(&lex)) {
            return (NklTokenArray){0};
        }
        nkda_append(&tokens, lex.token);

        // if (nkl_getErrorCount()) {
        //     return {};
        // }

#ifdef ENABLE_LOGGING
        NKSB_FIXED_BUFFER(sb, 256);
        nks_escape(nksb_getStream(&sb), nkl_getTokenStr(&lex.token, text));
        NK_LOG_DBG("%u: \"" NKS_FMT "\" @%u:%u", lex.token.id, NKS_ARG(sb), lex.token.lin, lex.token.col);
#endif // ENABLE_LOGGING
    } while (lex.token.id != NklBaseToken_Eof);

    return (NklTokenArray){NK_SLICE_INIT(tokens)};
}
