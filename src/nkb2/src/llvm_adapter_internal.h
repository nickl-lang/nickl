#pragma once

#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>

#ifdef __cplusplus
extern "C" {
#endif

void *lookupSymbol(LLVMOrcLLJITRef jit, LLVMOrcJITDylibRef jd, char const *name);

#ifdef __cplusplus
}
#endif
