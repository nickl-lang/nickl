#ifndef HEADER_GUARD_NKB_LEXER
#define HEADER_GUARD_NKB_LEXER

#include "nk/common/allocator.h"
#include "nk/common/array.hpp"
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

typedef struct {
    NkArray<NkIrToken> tokens;
    nkstr error_msg;
    bool ok;
} NkIrLexerState;

void nkir_lex(NkIrLexerState *lexer, NkAllocator tmp_alloc, nkstr src);

#endif // HEADER_GUARD_NKB_LEXER
