#ifndef NKL_CORE_NICKL_H_
#define NKL_CORE_NICKL_H_

#include <stdarg.h>

#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklState_T *NklState;

typedef NklTokenArray (*NklLexerProc)(void *data, NklState nkl, NkAllocator alloc, NkString text);

typedef struct {
    void *data;
    NklLexerProc proc;
} NklLexer;

typedef NklAstNodeArray (
    *NklParserProc)(void *data, NklState nkl, NkAllocator alloc, NkString text, NklTokenArray tokens);

typedef struct {
    void *data;
    NklParserProc proc;
} NklParser;

NklState nkl_state_create(NklLexer lexer, NklParser parser);
void nkl_state_free(NklState nkl);

NklSource nkl_getSource(NklState nkl, NkAtom file);

typedef struct NklError {
    struct NklError *next;
    NkString msg;
    NklToken const *token;
} NklError;

usize nkl_getErrorCount(NklState nkl);
NklError *nkl_getErrorList(NklState nkl);

NK_PRINTF_LIKE(3, 4) i32 nkl_reportError(NklState nkl, NklToken const *token, char const *fmt, ...);
i32 nkl_vreportError(NklState nkl, NklToken const *token, char const *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_H_
