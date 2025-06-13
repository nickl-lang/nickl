#pragma once

#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Types.h>

#include "nkb/ir.h"
#include "ntk/arena.h"
#include "ntk/atom.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkLlvmState_T *NkLlvmState;

typedef struct NkLlvmModule_T *NkLlvmModule;
typedef struct NkLlvmTarget_T *NkLlvmTarget;

typedef struct NkLlvmRuntime_T *NkLlvmRuntime;
typedef struct NkLlvmRuntimeModule_T *NkLlvmRuntimeModule;

typedef enum {
    NkLlvmOptLevel_O0,
    NkLlvmOptLevel_O1,
    NkLlvmOptLevel_O2,
    NkLlvmOptLevel_O3,
    NkLlvmOptLevel_Os,
    NkLlvmOptLevel_Oz,
} NkLlvmOptLevel;

typedef struct NkLlvmState_T {
    LLVMContextRef ctx;
    char *triple;
    LLVMTargetMachineRef tm;
} NkLlvmState_T;

typedef struct NkLlvmRuntime_T {
    LLVMOrcLLJITRef jit;
    LLVMOrcThreadSafeContextRef tsc;
    char *triple;
    LLVMTargetMachineRef tm;
} NkLlvmRuntime_T;

typedef struct NkLlvmRuntimeModule_T {
    LLVMOrcJITDylibRef jd;
} NkLlvmRuntimeModule_T;

NkLlvmState nk_llvm_newState(NkArena *arena);
void nk_llvm_freeState(NkLlvmState llvm);

void nk_llvm_initRuntime(NkLlvmRuntime rt);
void nk_llvm_freeRuntime(NkLlvmRuntime rt);

void nk_llvm_initRuntimeModule(NkLlvmRuntime rt, NkLlvmRuntimeModule mod);

void nk_llvm_emitObjectFile(NkArena *arena, NkLlvmState llvm, NkIrSymbolArray syms, NkString obj_file);

void nk_llvm_defineExternSymbols(
    NkArena *arena,
    NkLlvmRuntime rt,
    NkLlvmRuntimeModule mod,
    NkIrSymbolAddressArray syms);

void *nk_llvm_getSymbolAddress(
    NkArena *arena,
    NkLlvmState llvm,
    NkLlvmRuntime rt,
    NkLlvmRuntimeModule mod,
    NkIrSymbolArray syms,
    NkAtom sym);

#ifdef __cplusplus
}
#endif
