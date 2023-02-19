#ifndef HEADER_GUARD_NKL_LANG_COMPILER
#define HEADER_GUARD_NKL_LANG_COMPILER

#include "nkl/lang/ast.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nkstr compiler_binary;
} NklCompilerConfig;

typedef struct NklCompiler_T *NklCompiler;

NklCompiler nkl_compiler_create(NklCompilerConfig config);
void nkl_compiler_free(NklCompiler c);

void nkl_compiler_run(NklCompiler c, NklAstNode root);
void nkl_compiler_runSrc(NklCompiler c, nkstr src);
void nkl_compiler_runFile(NklCompiler c, nkstr path);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_COMPILER
