#ifndef HEADER_GUARD_NKB_LEXER
#define HEADER_GUARD_NKB_LEXER

#include "nk/common/allocator.h"
#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

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
