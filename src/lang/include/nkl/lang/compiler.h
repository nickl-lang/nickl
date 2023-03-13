#ifndef HEADER_GUARD_NKL_LANG_COMPILER
#define HEADER_GUARD_NKL_LANG_COMPILER

#include "nk/common/utils.h"
#include "nkl/lang/ast.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklCompiler_T *NklCompiler;

NK_EXPORT NklCompiler nkl_compiler_create();
NK_EXPORT void nkl_compiler_free(NklCompiler c);

NK_EXPORT bool nkl_compiler_configure(NklCompiler c, nkstr config_dir);

NK_EXPORT bool nkl_compiler_run(NklCompiler c, NklAstNode root);
NK_EXPORT bool nkl_compiler_runSrc(NklCompiler c, nkstr src);
NK_EXPORT bool nkl_compiler_runFile(NklCompiler c, nkstr path);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_COMPILER
