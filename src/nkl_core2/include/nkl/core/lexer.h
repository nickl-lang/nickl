#ifndef NKL_CORE_LEXER_H_
#define NKL_CORE_LEXER_H_

#include "nkl/common/token.h"
#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NklBaseToken_Eof = 0,

    NklBaseToken_Id,
    NklBaseToken_Int,
    NklBaseToken_Float,
    NklBaseToken_String,
    NklBaseToken_EscapedString,

    NklBaseToken_Error,

    NklBaseToken_Count,
};

typedef struct {
    NkString text;
    NkArena *arena;
    NkString *error;

    char const **keywords;
    char const **operators;
    char const *tag_prefixes;

    u32 keywords_base;
    u32 operators_base;
    u32 tags_base;
} NklLexerData;

bool nkl_lex(NklLexerData const *data, NklTokenArray *out_tokens);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_LEXER_H_
