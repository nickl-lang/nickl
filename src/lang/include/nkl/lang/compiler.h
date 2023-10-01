#ifndef HEADER_GUARD_NKL_LANG_COMPILER
#define HEADER_GUARD_NKL_LANG_COMPILER

#include "nkl/lang/ast.h"
#include "ntk/common.h"
#include "ntk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklCompiler_T *NklCompiler;

NklCompiler nkl_compiler_create();
void nkl_compiler_free(NklCompiler c);

bool nkl_compiler_configure(NklCompiler c, nkstr config_dir);

bool nkl_compiler_run(NklCompiler c, NklAstNode root);
bool nkl_compiler_runSrc(NklCompiler c, nkstr src);
bool nkl_compiler_runFile(NklCompiler c, nkstr path);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_COMPILER
