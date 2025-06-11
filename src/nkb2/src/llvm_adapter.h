#pragma once

#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Types.h>
#include <nkb/ir.h>
#include <ntk/arena.h>
#include <ntk/common.h>

#ifdef __cplusplus
extern "C" {
#endif

void *nk_llvm_lookup(LLVMOrcLLJITRef jit_raw, LLVMOrcJITDylibRef jd, char const *name);

LLVMModuleRef nk_llvm_createModule(NkArena *scratch1, NkArena *scratch2, NkIrSymbolArray ir, LLVMContextRef ctx);

bool nk_llvm_optimize(LLVMModuleRef module, LLVMTargetMachineRef tm, LLVMPassBuilderOptionsRef pbo);

#ifdef __cplusplus
}
#endif
