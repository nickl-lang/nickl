#ifndef HEADER_GUARD_NKB_LEXER
#define HEADER_GUARD_NKB_LEXER

#include "nk/common/allocator.h"
#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    NkIrToken const *data;
    size_t size;
} NkIrTokenArray;

typedef struct {
    NkIrTokenArray tokens;
    nkstr error_msg;
    bool ok;
} NkIrLexerResult;

NkIrLexerResult nkir_lex(NkAllocator alloc, nkstr src);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_LEXER
