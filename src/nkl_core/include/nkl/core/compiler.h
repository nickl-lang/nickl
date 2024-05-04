#ifndef NKL_CORE_COMPILER_H_
#define NKL_CORE_COMPILER_H_

#include "nkl/common/ast.h"
#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkAtom arch;
    NkAtom vendor;
    NkAtom sys;
} NklTargetTriple;

typedef struct NklCompiler_T *NklCompiler;
typedef struct NklModule_T *NklModule;

NklCompiler nkl_createCompiler(NklState nkl, NklTargetTriple target);
void nkl_freeCompiler(NklCompiler c);

NklModule nkl_createModule(NklCompiler c);

void nkl_writeModule(NklModule m, NkString filename);

bool nkl_compileFile(NklModule m, NkString filename);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_COMPILER_H_
