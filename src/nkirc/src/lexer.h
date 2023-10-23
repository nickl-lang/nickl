#ifndef HEADER_GUARD_NKIRC_LEXER
#define HEADER_GUARD_NKIRC_LEXER

#include "nkl/common/token.h"
#include "ntk/allocator.h"
#include "ntk/array.h"
#include "ntk/string.h"

#ifndef __cplusplus
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
    NklTokenArray tokens;
    nks error_msg;
    bool ok;
} NkIrLexerState;

void nkir_lex(NkIrLexerState *lexer, NkArena *file_arena, NkArena *tmp_arena, nks src);

#ifndef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKIRC_LEXER
