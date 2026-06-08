#pragma once

#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Types.h>

#include "llvm_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkLlvmJitState_T {
    LLVMOrcLLJITRef lljit;
    LLVMOrcThreadSafeContextRef tsc;
    LLVMTargetMachineRef tm;
} NkLlvmJitState_T;

void *tryLookupSymbol(LLVMOrcLLJITRef lljit, LLVMOrcJITDylibRef jd, NkString name);
void *lookupSymbol(LLVMOrcLLJITRef lljit, LLVMOrcJITDylibRef jd, NkString name);

#ifdef __cplusplus
}
#endif
