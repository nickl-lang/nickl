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

typedef enum {
    NklToken_Eof = 0,

    NklToken_Empty,
    NklToken_Unknown,

    NklToken_Id,
    NklToken_Int,
    NklToken_Float,
    NklToken_String,

    NklTokenId_Count,
} NklTokenId;

typedef NkSlice(NkString) NkStringArray;

typedef struct {
    NkStringArray keywords;
    NkStringArray operators;
    u32 keywords_id_base;
    u32 operators_id_base;
} NklLexerData;

NklTokenArray nkl_lex(NklLexerData const *data, NkArena *arena, NkString text);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_LEXER_H_
