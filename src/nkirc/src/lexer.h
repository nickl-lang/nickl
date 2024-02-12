#ifndef NKIRC_LEXER_H_
#define NKIRC_LEXER_H_

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

    NkIrToken_Count,
} ENkIrTokenId;

extern const char *s_token_id[];
extern const char *s_token_text[];

typedef struct {
    NklTokenDynArray tokens;
    NkString error_msg;
    bool ok;
} NkIrLexerState;

void nkir_lex(NkIrLexerState *lexer, NkArena *file_arena, NkArena *tmp_arena, NkString text);

#ifdef __cplusplus
}
#endif

#endif // NKIRC_LEXER_H_
