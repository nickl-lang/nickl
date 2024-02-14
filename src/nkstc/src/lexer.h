#ifndef NKSTC_LEXER_H_
#define NKSTC_LEXER_H_

#include "nkl/common/token.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
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

typedef struct {
    NklTokenDynArray tokens;
    NkString error_msg;
} NkStLexerState;

bool nkst_lex(NkStLexerState *lexer, NkArena *file_arena, NkArena *tmp_arena, NkAtom file, NkString text);

#ifdef __cplusplus
}
#endif

#endif // NKSTC_LEXER_H_
