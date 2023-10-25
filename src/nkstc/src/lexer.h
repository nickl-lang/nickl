#ifndef HEADER_GUARD_NKSTC_LEXER
#define HEADER_GUARD_NKSTC_LEXER

#include "nkl/common/token.h"
#include "ntk/allocator.h"
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
} ENkStTokenId;

extern const char *s_nkst_token_id[];
extern const char *s_nkst_token_text[];

typedef struct {
    NklTokenArray tokens;
    nks error_msg;
} NkStLexerState;

bool nkst_lex(NkStLexerState *lexer, NkArena *file_arena, NkArena *tmp_arena, nkid file, nks src);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKSTC_LEXER
