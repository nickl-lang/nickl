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
    NklToken_Eof = 0,

    NklToken_Id,
    NklToken_Int,
    NklToken_IntHex,
    NklToken_Float,
    NklToken_String,
    NklToken_EscapedString,
    NklToken_Newline,

    NklToken_Error,

    NklToken_Count,
};

typedef struct {
    NkString text;
    NkArena *arena;

    NkString *err_str;

    char const **tokens;
    u32 keywords_base;
    u32 operators_base;
    u32 tags_base;
} NklLexerData;

bool nkl_lex(NklLexerData const *data, NklTokenArray *out_tokens);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_LEXER_H_
