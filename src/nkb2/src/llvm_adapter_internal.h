#pragma once

#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Types.h>

#include "llvm_adapter.h"
#include "ntk/arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkLlvmState_T {
    NkArena *arena;
    LLVMContextRef ctx;
} NkLlvmState_T;

typedef struct NkLlvmJitState_T {
    LLVMOrcLLJITRef lljit;
    LLVMOrcThreadSafeContextRef tsc;
    LLVMTargetMachineRef tm;
} NkLlvmJitState_T;

void *lookupSymbol(LLVMOrcLLJITRef jit, LLVMOrcJITDylibRef jd, char const *name);

#ifdef __cplusplus
}
#endif
