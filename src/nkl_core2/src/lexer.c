#include "nkl/core/lexer.h"

#include <ctype.h>

#include "nkl/common/token.h"
#include "ntk/arena.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(lexer);

typedef struct {
    NkString const text;
    NkArena *arena;
    NkString *error;

    char const **const keywords;
    char const **const operators;
    char const *const tag_prefixes;

    u32 const first_keyword_id;
    u32 const first_operator_id;
    u32 const first_tag_id;

    u32 pos;
    u32 lin;
    u32 col;
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

static int onPrint(LexerState const *lex, i64 offset) {
    return isprint(chr(lex, offset));
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

static void accept(LexerState *lex, NklToken *token, i64 n) {
    advance(lex, n);
    token->len += n;
}

static void discard(LexerState *lex, NklToken *token) {
    lex->pos -= token->len;
    lex->col -= token->len;
    token->len = 0;
}

static void skipSpaces(LexerState *lex) {
    while (onSpace(lex, 0)) {
        advance(lex, 1);
    }
}

static i32 vreportError(LexerState *lex, char const *fmt, va_list ap) {
    i32 res = 0;
    if (lex->error) {
        *lex->error = nk_vtsprintf(lex->arena, fmt, ap);
    }
    return res;
}

NK_PRINTF_LIKE(2) static i32 reportError(LexerState *lex, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    i32 res = vreportError(lex, fmt, ap);
    va_end(ap);

    return res;
}

static NklToken scan(LexerState *lex) {
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

    NklToken token = {
        .id = NklBaseToken_Error,
        .pos = lex->pos,
        .len = 0,
        .lin = lex->lin,
        .col = lex->col,
    };

    if (!chr(lex, 0)) {
        token.id = NklBaseToken_Eof;
        return token;
    }

    if (on(lex, '"', 0)) {
        accept(lex, &token, 1);

        bool escaped = false;

        while (chr(lex, 0) && !on(lex, '\"', 0) && !on(lex, '\n', 0)) {
            accept(lex, &token, 1);
            if (on(lex, '\\', -1)) {
                switch (chr(lex, 0)) {
                    case 'n':
                    case 't':
                    case '0':
                    case '\\':
                    case '"':
                    case '\n':
                        escaped = true;
                        accept(lex, &token, 1);
                        break;
                    default:
                        if (!chr(lex, 0)) {
                            reportError(lex, "unexpected end of file");
                            return token;
                        } else {
                            token.pos = lex->pos - 1;
                            token.len = 2;
                            token.lin = lex->lin;
                            token.col = lex->col - 1;
                            if (onPrint(lex, 0)) {
                                reportError(lex, "invalid escape sequence `\\%c`", chr(lex, 0));
                                return token;
                            } else {
                                reportError(lex, "invalid escape sequence `\\\\x%" PRIx8 "`", chr(lex, 0) & 0xff);
                                return token;
                            }
                        }
                }
            }
        }

        if (!on(lex, '\"', 0)) {
            accept(lex, &token, 1);
            reportError(lex, "invalid string constant");
            return token;
        }

        token.id = escaped ? NklBaseToken_EscapedString : NklBaseToken_String;

        accept(lex, &token, 1);
        return token;
    }

    if (onDigit(lex, on(lex, '-', 0) ? 1 : 0)) {
        if (on(lex, '-', 0)) {
            accept(lex, &token, 1);
        }

        token.id = NklBaseToken_Int;

        while (onDigit(lex, 0)) {
            accept(lex, &token, 1);
        }

        if (on(lex, '.', 0)) {
            token.id = NklBaseToken_Float;
            do {
                accept(lex, &token, 1);
            } while (onDigit(lex, 0));
        }

        if (onLower(lex, 0) == 'e') {
            token.id = NklBaseToken_Float;
            accept(lex, &token, 1);
            if (on(lex, '-', 0) || on(lex, '+', 0)) {
                accept(lex, &token, 1);
            }
            if (!onDigit(lex, 0)) {
                reportError(lex, "invalid float literal");
                return token;
            }
            while (onDigit(lex, 0)) {
                accept(lex, &token, 1);
            }
        }

        if (onAlpha(lex, 0)) {
            accept(lex, &token, 1);
            reportError(lex, "invalid suffix");
            return token;
        }

        return token;
    }

    if (onAlphaOrUscr(lex, 0)) {
        while (onAlnumOrUscr(lex, 0)) {
            accept(lex, &token, 1);
        }

        NkString const token_str = nkl_getTokenStr(&token, lex->text);
        char const **it = lex->keywords;
        for (; it && *it && !nks_equalCStr(token_str, *it); it++) {
        }

        if (it && *it) {
            ptrdiff_t const idx = it - lex->keywords;
            token.id = lex->first_keyword_id + idx;
        } else {
            token.id = NklBaseToken_Id;
        }

        return token;
    }

    {
        char const *it = lex->tag_prefixes;
        for (; it && *it && !(on(lex, *it, 0) && onAlphaOrUscr(lex, 1)); it++) {
        }

        if (it && *it) {
            do {
                accept(lex, &token, 1);
            } while (onAlnumOrUscr(lex, 0));

            ptrdiff_t const idx = it - lex->tag_prefixes;
            token.id = lex->first_tag_id + idx;
            return token;
        }
    }

    {
        char const **it = lex->operators;
        for (; it && *it; it++) {
            char const *op = *it;

            while (*op && on(lex, *op, 0)) {
                accept(lex, &token, 1);
                op++;
            }

            if (*op) {
                discard(lex, &token);
            } else {
                break;
            }
        }

        if (it && *it) {
            ptrdiff_t const idx = it - lex->operators;
            token.id = lex->first_operator_id + idx;
            return token;
        }
    }

    if (onPrint(lex, 0)) {
        reportError(lex, "unexpected character `%c`", chr(lex, 0));
        return token;
    } else {
        reportError(lex, "unexpected byte `\\x%" PRIx8 "`", chr(lex, 0) & 0xff);
        return token;
    }
}

bool nkl_lex(NklLexerData const *data, NklTokenArray *out_tokens) {
    NK_LOG_TRC("%s", __func__);

    bool ret;
    NK_PROF_FUNC() {
        NklTokenDynArray tokens = {NKDA_INIT(nk_arena_getAllocator(data->arena))};
        nkda_reserve(&tokens, 1000);

        LexerState lex = {
            .text = data->text,
            .arena = data->arena,
            .error = data->error,

            .keywords = data->keywords,
            .operators = data->operators,
            .tag_prefixes = data->tag_prefixes,

            .first_keyword_id = data->keywords_base + 1,
            .first_operator_id = data->operators_base + 1,
            .first_tag_id = data->tags_base + 1,

            .pos = 0,
            .lin = 1,
            .col = 1,
        };

        NklToken token;
        do {
            token = scan(&lex);
            nkda_append(&tokens, token);

#ifdef ENABLE_LOGGING
            NKSB_FIXED_BUFFER(sb, 256);
            nks_escape(nksb_getStream(&sb), nkl_getTokenStr(&token, data->text));
            NK_LOG_DBG("%u:%u: \"" NKS_FMT "\":%u", token.lin, token.col, NKS_ARG(sb), token.id);
#endif // ENABLE_LOGGING
        } while (token.id != NklBaseToken_Error && token.id != NklBaseToken_Eof);

        *out_tokens = (NklTokenArray){NK_SLICE_INIT(tokens)};
        ret = token.id == NklBaseToken_Eof;
    }

    return ret;
}
