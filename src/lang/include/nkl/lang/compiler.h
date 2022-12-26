#ifndef HEADER_GUARD_NKL_LANG_COMPILER
#define HEADER_GUARD_NKL_LANG_COMPILER

#include "nkl/lang/ast.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklCompiler_T *NklCompiler;

NklCompiler nkl_compiler_create();
void nkl_compiler_free(NklCompiler c);

void nkl_run(NklCompiler c, NklAstNode root);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_COMPILER
