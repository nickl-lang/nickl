#ifndef NKL_CORE_NICKL_IMPL_H_
#define NKL_CORE_NICKL_IMPL_H_

#include "nkb/ir.h"
#include "nkl/common/ast.h"
#include "nkl/common/diagnostics.h"
#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklState_T {
    struct NklState_T *next;

    NkArena arena;
    NkArena scratch;
    NklError *error;
} NklState_T;

typedef struct {
    NkString lib;
    NkString alias;
} LibAlias;

typedef struct NklCompiler_T {
    NklState nkl;

    NkDynArray(LibAlias) lib_aliases;
} NklCompiler_T;

typedef struct NklModule_T {
    NklCompiler com;
    NkIrSymbolDynArray ir;

    NkDynArray(LibAlias) lib_aliases;
} NklModule_T;

extern _Thread_local NklState s_nkl;

extern char const *s_ir_tokens[];
extern char const *s_ast_tokens[];

NK_PRINTF_LIKE(3) void nickl_reportError(NklState nkl, NklSourceLocation loc, char const *fmt, ...);
void nickl_vreportError(NklState nkl, NklSourceLocation loc, char const *fmt, va_list ap);

bool nickl_getText(NklState nkl, NkAtom file, NkString *out_text);

bool nickl_getTokensIr(NklState nkl, NkAtom file, NklTokenArray *out_tokens);
bool nickl_getTokensAst(NklState nkl, NkAtom file, NklTokenArray *out_tokens);

bool nickl_getAst(NklState nkl, NkAtom file, NklAstNodeArray *out_nodes);

NkAtom nickl_canonicalizePath(NkString base, NkString path);
NkAtom nickl_findFile(NklState nkl, NkAtom base, NkString name);

NkString nickl_translateLib(NklModule mod, NkString alias);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_IMPL_H_
