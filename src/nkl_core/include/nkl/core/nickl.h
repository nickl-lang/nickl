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

typedef NkSlice(NkString) StringSlice;

typedef struct NklState_T *NklState;

typedef NklTokenArray (*NklLexerProc)(NklState nkl, NkAllocator alloc, NkAtom file, NkString text);
typedef NklAstNodeArray (
    *NklParserProc)(NklState nkl, NkAllocator alloc, NkAtom file, NkString text, NklTokenArray tokens);

NK_EXPORT NklState nkl_state_create(StringSlice args, NklLexerProc lexer_proc, NklParserProc parser_proc);
NK_EXPORT void nkl_state_free(NklState nkl);

NK_EXPORT NklSource const *nkl_getSource(NklState nkl, NkAtom file);

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

NK_EXPORT void nkl_errorStateEquip(NklErrorState *state);
NK_EXPORT void nkl_errorStateUnequip(void);

NK_EXPORT usize nkl_getErrorCount(void);

NK_EXPORT NK_PRINTF_LIKE(3) i32 nkl_reportError(NkAtom file, NklToken const *token, char const *fmt, ...);
NK_EXPORT i32 nkl_vreportError(NkAtom file, NklToken const *token, char const *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_H_
