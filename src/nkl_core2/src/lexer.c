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
    NkArena *const arena;
    NkString *const err_str;

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

static char chr(LexerState const *l, i64 offset) {
    return l->pos + offset < (u32)l->text.size ? l->text.data[l->pos + offset] : '\0';
}

static bool on(LexerState const *l, char c, i64 offset) {
    return chr(l, offset) == c;
}

static int onSpace(LexerState const *l, i64 offset) {
    return isspace(chr(l, offset));
}

static int onAlpha(LexerState const *l, i64 offset) {
    return isalpha(chr(l, offset));
}

static int onAlnum(LexerState const *l, i64 offset) {
    return isalnum(chr(l, offset));
}

static int onDigit(LexerState const *l, i64 offset) {
    return isdigit(chr(l, offset));
}

static int onLower(LexerState const *l, i64 offset) {
    return tolower(chr(l, offset));
}

static bool onAlphaOrUscr(LexerState const *l, i64 offset) {
    return onAlpha(l, offset) || on(l, '_', offset);
}

static bool onAlnumOrUscr(LexerState const *l, i64 offset) {
    return onAlnum(l, offset) || on(l, '_', offset);
}

static int onPrint(LexerState const *l, i64 offset) {
    return isprint(chr(l, offset));
}

static void advance(LexerState *l, i64 n) {
    for (i64 i = 0; i < n && chr(l, 0); i++) {
        if (on(l, '\n', 0)) {
            l->col = 1;
            l->lin++;
        } else {
            l->col++;
        }
        l->pos++;
    }
}

static void accept(LexerState *l, NklToken *token, i64 n) {
    advance(l, n);
    token->len += n;
}

static void discard(LexerState *l, NklToken *token) {
    l->pos -= token->len;
    l->col -= token->len;
    token->len = 0;
}

static void skipSpaces(LexerState *l) {
    while (onSpace(l, 0) && !on(l, '\n', 0)) {
        advance(l, 1);
    }
}

static i32 vreportError(LexerState *l, char const *fmt, va_list ap) {
    i32 res = 0;
    if (l->err_str) {
        *l->err_str = nk_vtsprintf(l->arena, fmt, ap);
    }
    return res;
}

NK_PRINTF_LIKE(2) static i32 reportError(LexerState *l, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    i32 res = vreportError(l, fmt, ap);
    va_end(ap);

    return res;
}

static NklToken scan(LexerState *l) {
    skipSpaces(l);

    if (l->pos == 0 && on(l, '#', 0) && on(l, '!', 1)) {
        while (chr(l, 0) && !on(l, '\n', 0)) {
            advance(l, 1);
        }
        skipSpaces(l);
    }

    while ((on(l, '/', 0) && on(l, '/', 1)) || (on(l, '/', 0) && on(l, '*', 1))) {
        if (on(l, '/', 1)) {
            while (!on(l, '\n', 0)) {
                advance(l, 1);
            }
        } else {
            advance(l, 2);
            while (chr(l, 0)) {
                if (on(l, '*', 0) && on(l, '/', 1)) {
                    advance(l, 2);
                    break;
                } else {
                    advance(l, 1);
                }
            }
        }
        skipSpaces(l);
    }

    NklToken token = {
        .id = NklToken_Error,
        .pos = l->pos,
        .len = 0,
        .lin = l->lin,
        .col = l->col,
    };

    if (!chr(l, 0)) {
        token.id = NklToken_Eof;
        return token;
    }

    if (on(l, '\n', 0)) {
        while (on(l, '\n', 0)) {
            accept(l, &token, 1);
        }
        token.id = NklToken_Newline;
        return token;
    }

    if (on(l, '"', 0)) {
        accept(l, &token, 1);

        bool escaped = false;

        while (chr(l, 0) && !on(l, '\"', 0) && !on(l, '\n', 0)) {
            accept(l, &token, 1);
            if (on(l, '\\', -1)) {
                switch (chr(l, 0)) {
                    case 'n':
                    case 't':
                    case '0':
                    case '\\':
                    case '"':
                    case '\n':
                        escaped = true;
                        accept(l, &token, 1);
                        break;
                    default:
                        if (!chr(l, 0)) {
                            reportError(l, "unexpected end of file");
                            return token;
                        } else {
                            token.pos = l->pos - 1;
                            token.len = 2;
                            token.lin = l->lin;
                            token.col = l->col - 1;
                            if (onPrint(l, 0)) {
                                reportError(l, "invalid escape sequence `\\%c`", chr(l, 0));
                                return token;
                            } else {
                                reportError(l, "invalid escape sequence `\\\\x%" PRIx8 "`", chr(l, 0) & 0xff);
                                return token;
                            }
                        }
                }
            }
        }

        if (!on(l, '\"', 0)) {
            accept(l, &token, 1);
            reportError(l, "invalid string constant");
            return token;
        }

        token.id = escaped ? NklToken_EscapedString : NklToken_String;

        accept(l, &token, 1);
        return token;
    }

    if (onDigit(l, on(l, '-', 0) ? 1 : 0)) {
        if (on(l, '-', 0)) {
            accept(l, &token, 1);
        }

        token.id = NklToken_Int;

        while (onDigit(l, 0)) {
            accept(l, &token, 1);
        }

        if (on(l, '.', 0)) {
            token.id = NklToken_Float;
            do {
                accept(l, &token, 1);
            } while (onDigit(l, 0));
        }

        if (onLower(l, 0) == 'e') {
            token.id = NklToken_Float;
            accept(l, &token, 1);
            if (on(l, '-', 0) || on(l, '+', 0)) {
                accept(l, &token, 1);
            }
            if (!onDigit(l, 0)) {
                reportError(l, "invalid float literal");
                return token;
            }
            while (onDigit(l, 0)) {
                accept(l, &token, 1);
            }
        }

        if (onAlpha(l, 0)) {
            accept(l, &token, 1);
            reportError(l, "invalid suffix");
            return token;
        }

        return token;
    }

    if (onAlphaOrUscr(l, 0)) {
        while (onAlnumOrUscr(l, 0)) {
            accept(l, &token, 1);
        }

        NkString const token_str = nkl_getTokenStr(&token, l->text);
        char const **it = l->keywords;
        for (; it && *it && !nks_equalCStr(token_str, *it); it++) {
        }

        if (it && *it) {
            ptrdiff_t const idx = it - l->keywords;
            token.id = l->first_keyword_id + idx;
        } else {
            token.id = NklToken_Id;
        }

        return token;
    }

    {
        char const *it = l->tag_prefixes;
        for (; it && *it && !(on(l, *it, 0) && onAlphaOrUscr(l, 1)); it++) {
        }

        if (it && *it) {
            do {
                accept(l, &token, 1);
            } while (onAlnumOrUscr(l, 0));

            ptrdiff_t const idx = it - l->tag_prefixes;
            token.id = l->first_tag_id + idx;
            return token;
        }
    }

    {
        char const **it = l->operators;
        for (; it && *it; it++) {
            char const *op = *it;

            while (*op && on(l, *op, 0)) {
                accept(l, &token, 1);
                op++;
            }

            if (*op) {
                discard(l, &token);
            } else {
                break;
            }
        }

        if (it && *it) {
            ptrdiff_t const idx = it - l->operators;
            token.id = l->first_operator_id + idx;
            return token;
        }
    }

    if (onPrint(l, 0)) {
        reportError(l, "unexpected character `%c`", chr(l, 0));
        return token;
    } else {
        reportError(l, "unexpected byte `\\x%" PRIx8 "`", chr(l, 0) & 0xff);
        return token;
    }
}

bool nkl_lex(NklLexerData const *data, NklTokenArray *out_tokens) {
    NK_LOG_TRC("%s", __func__);

    bool ret;
    NK_PROF_FUNC() {
        NklTokenDynArray tokens = {NKDA_INIT(nk_arena_getAllocator(data->arena))};
        nkda_reserve(&tokens, 1000);

        LexerState l = {
            .text = data->text,
            .arena = data->arena,
            .err_str = data->err_str,

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
            token = scan(&l);
            nkda_append(&tokens, token);

#ifdef ENABLE_LOGGING
            NKSB_FIXED_BUFFER(sb, 256);
            nks_escape(nksb_getStream(&sb), nkl_getTokenStr(&token, data->text));
            NK_LOG_DBG("%u:%u: \"" NKS_FMT "\":%u", token.lin, token.col, NKS_ARG(sb), token.id);
#endif // ENABLE_LOGGING
        } while (token.id != NklToken_Error && token.id != NklToken_Eof);

        *out_tokens = (NklTokenArray){NK_SLICE_INIT(tokens)};
        ret = token.id == NklToken_Eof;
    }

    return ret;
}
