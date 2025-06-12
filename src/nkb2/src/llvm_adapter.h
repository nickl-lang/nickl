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
typedef struct NkLlvmRuntime_T *NkLlvmRuntime;
typedef struct NkLlvmRuntimeModule_T *NkLlvmRuntimeModule;

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

void nk_llvm_init(NkLlvmState llvm);
void nk_llvm_free(NkLlvmState llvm);

void nk_llvm_initRuntime(NkLlvmRuntime rt);
void nk_llvm_freeRuntime(NkLlvmRuntime rt);

void nk_llvm_initRuntimeModule(NkLlvmRuntime rt, NkLlvmRuntimeModule mod);

void nk_llvm_emitObjectFile(NkArena (*scratch)[2], NkLlvmState llvm, NkIrSymbolArray syms, NkString obj_file);

void nk_llvm_defineExternSymbols(
    NkArena *scratch,
    NkLlvmRuntime rt,
    NkLlvmRuntimeModule mod,
    NkIrSymbolAddressArray syms);

void *nk_llvm_getSymbolAddress(
    NkArena (*scratch)[2],
    NkLlvmState llvm,
    NkLlvmRuntime rt,
    NkLlvmRuntimeModule mod,
    NkIrSymbolArray syms,
    NkAtom sym);

#ifdef __cplusplus
}
#endif
