#ifndef NKSTC_COMPILER_H_
#define NKSTC_COMPILER_H_

#include "nkl/common/ast.h"
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

NklCompiler nkl_createCompiler(NklTargetTriple target);
void nkl_freeCompiler(NklCompiler c);

NklModule nkl_createModule(NklCompiler c);

void nkl_writeModule(NklModule m, NkString filename);

void nkl_compile(NklModule m, NklSource src);

#ifdef __cplusplus
}
#endif

#endif // NKSTC_COMPILER_H_
