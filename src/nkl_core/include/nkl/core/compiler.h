#ifndef NKL_CORE_COMPILER_H_
#define NKL_CORE_COMPILER_H_

#include "nkb/ir.h"
#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/common.h"
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

NK_EXPORT NklCompiler nkl_createCompiler(NklState nkl, NklTargetTriple target);
NK_EXPORT void nkl_freeCompiler(NklCompiler c);

NK_EXPORT usize nkl_getCompileErrorCount(NklCompiler c);
NK_EXPORT NklError *nkl_getCompileErrorList(NklCompiler c);

NK_EXPORT NklModule nkl_createModule(NklCompiler c);

NK_EXPORT bool nkl_writeModule(NklModule m, NkIrCompilerConfig conf);

NK_EXPORT bool nkl_compileFile(NklModule m, NkString filename);
NK_EXPORT bool nkl_runFile(NklModule m, NkString filename);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_COMPILER_H_
