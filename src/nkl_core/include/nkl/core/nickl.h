#ifndef NKL_CORE_NICKL_H_
#define NKL_CORE_NICKL_H_

#include <stdarg.h>

#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "ntk/arena.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklState_T *NklState;

typedef NklTokenArray (*NklLexerProc)(NklState nkl, NkAllocator alloc, NkAtom file, NkString text);
typedef NklAstNodeArray (
    *NklParserProc)(NklState nkl, NkAllocator alloc, NkAtom file, NkString text, NklTokenArray tokens);

NklState nkl_state_create(NklLexerProc lexer_proc, NklParserProc parser_proc);
void nkl_state_free(NklState nkl);

NklSource nkl_getSource(NklState nkl, NkAtom file);

typedef struct NklError {
    struct NklError *next;

    NkString msg;
    NkAtom file;
    NklToken const *token;
} NklError;

typedef struct NklErrorState {
    struct NklErrorState *next;

    NkArena *arena;
    NklError *errors;
    NklError *last_error;
    usize error_count;
} NklErrorState;

void nkl_errorStateEquip(NklErrorState *state);
void nkl_errorStateUnequip(void);

usize nkl_getErrorCount(void);

NK_PRINTF_LIKE(3, 4) i32 nkl_reportError(NkAtom file, NklToken const *token, char const *fmt, ...);
i32 nkl_vreportError(NkAtom file, NklToken const *token, char const *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_H_
