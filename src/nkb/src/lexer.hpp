#ifndef HEADER_GUARD_NKB_LEXER
#define HEADER_GUARD_NKB_LEXER

#include "nk/common/allocator.h"
#include "nk/common/array.h"
#include "nk/common/string.h"

enum ETokenId {
#define OP(ID, TEXT) t_##ID,
#define KW(ID) t_##ID,
#define SP(ID, TEXT) t_##ID,
#include "tokens.inl"

    Token_Count,
};

typedef struct {
    nkstr text;
    size_t id;
    size_t lin;
    size_t col;
} NkIrToken;

extern const char *s_token_id[];
extern const char *s_token_text[];

nkar_typedef(NkIrToken, NkIrTokenArray);
nkav_typedef(NkIrToken, NkIrTokenView);

typedef struct {
    NkIrTokenArray tokens;
    nkstr error_msg;
    bool ok;
} NkIrLexerState;

void nkir_lex(NkIrLexerState *lexer, NkArena *file_arena, NkArena *tmp_arena, nkstr src);

#endif // HEADER_GUARD_NKB_LEXER
