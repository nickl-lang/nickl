#ifndef NKL_LANG_COMPILER_H_
#define NKL_LANG_COMPILER_H_

#include "nkl/lang/ast.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NklCompiler_T *NklCompiler;

NklCompiler nkl_compiler_create();
void nkl_compiler_free(NklCompiler c);

bool nkl_compiler_configure(NklCompiler c, NkString self_path);

bool nkl_compiler_run(NklCompiler c, NklAstNode root);
bool nkl_compiler_runSrc(NklCompiler c, NkString src);
bool nkl_compiler_runFile(NklCompiler c, NkString path);

#ifdef __cplusplus
}
#endif

#endif // NKL_LANG_COMPILER_H_
