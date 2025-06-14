#pragma once

#include "nkb/ir.h"
#include "ntk/arena.h"
#include "ntk/atom.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkLlvmState_T *NkLlvmState;

typedef struct NkLlvmModule_T *NkLlvmModule;
typedef struct NkLlvmTarget_T *NkLlvmTarget;

typedef struct NkLlvmJitState_T *NkLlvmJitState;
typedef struct NkLlvmJitDylib_T *NkLlvmJitDylib;

typedef enum {
    NkLlvmOptLevel_O0,
    NkLlvmOptLevel_O1,
    NkLlvmOptLevel_O2,
    NkLlvmOptLevel_O3,
    NkLlvmOptLevel_Os,
    NkLlvmOptLevel_Oz,
} NkLlvmOptLevel;

NkLlvmState nk_llvm_createState(NkArena *arena);
void nk_llvm_freeState(NkLlvmState llvm);

NkLlvmJitState nk_llvm_createJitState(NkLlvmState llvm);
void nk_llvm_freeJitState(NkLlvmJitState jit);

NkLlvmTarget nk_llvm_getJitTarget(NkLlvmJitState jit);

NkLlvmJitDylib nk_llvm_createJitDylib(NkLlvmState llvm, NkLlvmJitState jit);

NkLlvmTarget nk_llvm_createTarget(NkLlvmState llvm, char const *triple);
void nk_llvm_freeTarget(NkLlvmTarget tgt);

NkLlvmModule nk_llvm_compilerIr(NkArena *scratch, NkLlvmState llvm, NkIrSymbolArray ir);
void nk_llvm_optimizeIr(NkArena *scratch, NkLlvmModule mod, NkLlvmTarget tgt, NkLlvmOptLevel opt);

void nk_llvm_defineExternSymbols(NkArena *scratch, NkLlvmJitState jit, NkLlvmJitDylib dl, NkIrSymbolAddressArray syms);

void nk_llvm_emitObjectFile(NkLlvmModule mod, NkLlvmTarget tgt, NkString obj_file);

void nk_llvm_jitModule(NkLlvmModule mod, NkLlvmJitState jit, NkLlvmJitDylib dl);
void *nk_llvm_getSymbolAddress(NkLlvmJitState jit, NkLlvmJitDylib dl, NkAtom sym);

#ifdef __cplusplus
}
#endif
