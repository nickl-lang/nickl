#ifndef NKSTC_LEXER_H_
#define NKSTC_LEXER_H_

#include "nkl/common/token.h"
#include "ntk/arena.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define OP(ID, TEXT) t_##ID,
#define KW(ID) t_##ID,
#define SP(ID, TEXT) t_##ID,
#include "tokens.inl"

    NkStTokenId_Count,
} NkStTokenId;

extern const char *s_nkst_token_id[];
extern const char *s_nkst_token_text[];

NklTokenArray nkst_lex(NkArena *arena, NkString text);

#ifdef __cplusplus
}
#endif

#endif // NKSTC_LEXER_H_
