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
    NkLlvmTarget tgt;
} NkLlvmJitState_T;

// TODO: Just pass through dylib and target refs
typedef struct NkLlvmJitDylib_T {
    LLVMOrcJITDylibRef jd;
} NkLlvmJitDylib_T;

typedef struct NkLlvmTarget_T {
    LLVMTargetMachineRef tm;
} NkLlvmTarget_T;

void *lookupSymbol(LLVMOrcLLJITRef jit, LLVMOrcJITDylibRef jd, char const *name);

#ifdef __cplusplus
}
#endif
