#ifndef NKL_CORE_LEXER_H_
#define NKL_CORE_LEXER_H_

#include "nkl/common/token.h"
#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/slice.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NklBaseToken_Eof = 0,

    NklBaseToken_Empty,
    NklBaseToken_Unknown,

    NklBaseToken_Id,
    NklBaseToken_Int,
    NklBaseToken_Float,
    NklBaseToken_String,

    NklBaseToken_Count,
};

typedef struct {
    char const **keywords;
    char const **operators;
    char const *tag_prefixes;
    u32 keywords_base;
    u32 operators_base;
    u32 tags_base;
} NklLexerData;

NklTokenArray nkl_lex(NklLexerData const *data, NkArena *arena, NkString text);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_LEXER_H_
