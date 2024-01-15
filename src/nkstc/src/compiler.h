#ifndef HEADER_GUARD_NKSTC_COMPILER
#define HEADER_GUARD_NKSTC_COMPILER

#include "nkl/common/ast.h"
#include "ntk/id.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nkid arch;
    nkid vendor;
    nkid sys;
} NklTargetTriple;

typedef struct NklCompiler_T *NklCompiler;
typedef struct NklModule_T *NklModule;

NklCompiler nkl_createCompiler(NklTargetTriple target);
void nkl_freeCompiler(NklCompiler c);

NklModule nkl_createModule(NklCompiler c);

void nkl_writeModule(NklModule m, nks filename);

void nkl_compile(NklModule m, NklSource src);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKSTC_COMPILER
