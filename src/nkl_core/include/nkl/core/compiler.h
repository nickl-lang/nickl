#ifndef NKL_CORE_COMPILER_H_
#define NKL_CORE_COMPILER_H_

#include "nkb/ir.h"
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

usize nkl_getCompileErrorCount(NklCompiler c);
NklError *nkl_getCompileErrorList(NklCompiler c);

NklModule nkl_createModule(NklCompiler c);

bool nkl_runModule(NklModule m);
bool nkl_writeModule(NklModule m, NkIrCompilerConfig conf);

bool nkl_compileFile(NklModule m, NkString filename);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_COMPILER_H_
