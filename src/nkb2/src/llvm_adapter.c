#include "llvm_adapter.h"

#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <unistd.h> // TODO: Remove, only used for _exit

#include "llvm_adapter_internal.h"
#include "llvm_emitter.h"
#include "ntk/arena.h"
#include "ntk/log.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(llvm_adapter);

static LLVMModuleRef compilerIr(NkArena *arena, NkLlvmState llvm, NkIrSymbolArray ir) {
    LLVMModuleRef module = NULL;
    NkStringBuilder llvm_ir = {.alloc = nk_arena_getAllocator(arena)};
    nk_llvm_emitIr(nksb_getStream(&llvm_ir), arena, ir);
    nksb_appendNull(&llvm_ir);
    llvm_ir.size--;

    NK_LOG_STREAM_INF {
        NkStream log = nk_log_getStream();
        nk_printf(log, "LLVM IR:\n" NKS_FMT, NKS_ARG(llvm_ir));
    }

    LLVMMemoryBufferRef buffer =
        LLVMCreateMemoryBufferWithMemoryRange(llvm_ir.data, llvm_ir.size, "main", 1); // TODO: hardcoded buffer name

    char *error = NULL;
    if (LLVMParseIRInContext(llvm->ctx, buffer, &module, &error)) {
        // TODO: Report errors properly
        NK_LOG_ERR("Error parsing IR: %s", error);
        _exit(1);
        return NULL;
    }
    return module;
}

static char optLevelChar(NkLlvmOptLevel opt) {
    switch (opt) {
        case NkLlvmOptLevel_O0:
            return '0';
        case NkLlvmOptLevel_O1:
            return '1';
        case NkLlvmOptLevel_O2:
            return '2';
        case NkLlvmOptLevel_O3:
            return '3';
        case NkLlvmOptLevel_Os:
            return 's';
        case NkLlvmOptLevel_Oz:
            return 'z';
    }
    return '0';
}

static void optimizeModule(NkArena *arena, LLVMModuleRef module, LLVMTargetMachineRef tm, NkLlvmOptLevel opt) {
    LLVMPassBuilderOptionsRef pbo = LLVMCreatePassBuilderOptions();

    LLVMErrorRef err = NULL;
    if ((err = LLVMRunPasses(module, nk_tprintf(arena, "default<O%c>", optLevelChar(opt)), tm, pbo))) {
        char *err_msg = LLVMGetErrorMessage(err);
        // TODO: Report errors properly
        NK_LOG_ERR("Failed to optimize: %s", err_msg);
        _exit(1);
        return;
    }

    NK_LOG_STREAM_INF {
        char *str = LLVMPrintModuleToString(module);
        NkStream log = nk_log_getStream();
        nk_printf(log, "Optimized LLVM IR:\n%s", str);
        LLVMDisposeMessage(str);
    }

    LLVMDisposePassBuilderOptions(pbo);
}

NkLlvmState nk_llvm_newState(NkArena *arena) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    char *error = NULL;

    char *triple = LLVMGetDefaultTargetTriple();
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
        // TODO: Report errors properly
        NK_LOG_ERR("Failed to get target from triple: %s", error);
        _exit(1);
        return NULL;
    }
    LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
        target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocPIC, LLVMCodeModelDefault);

    NkLlvmState llvm = nk_arena_allocT(arena, NkLlvmState_T);
    *llvm = (NkLlvmState_T){
        .ctx = LLVMContextCreate(),
        .triple = triple,
        .tm = tm,
    };
    return llvm;
}

void nk_llvm_freeState(NkLlvmState llvm) {
    LLVMDisposeTargetMachine(llvm->tm);
    LLVMDisposeMessage(llvm->triple);

    LLVMContextDispose(llvm->ctx);
}

void nk_llvm_initRuntime(NkLlvmRuntime rt) {
    LLVMErrorRef err = NULL;
    char *error = NULL;

    LLVMOrcThreadSafeContextRef tsc = LLVMOrcCreateNewThreadSafeContext();

    LLVMOrcLLJITBuilderRef builder = LLVMOrcCreateLLJITBuilder();
    LLVMOrcLLJITRef jit;
    if ((err = LLVMOrcCreateLLJIT(&jit, builder))) {
        char *err_msg = LLVMGetErrorMessage(err);
        NK_LOG_ERR("Failed to create jit: %s", err_msg);
        LLVMDisposeErrorMessage(err_msg);
    }

    char *triple = LLVMGetDefaultTargetTriple();

    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
        // TODO: Report errors properly
        NK_LOG_ERR("Failed to get target from triple: %s", error);
        LLVMDisposeMessage(error);
        _exit(1);
        return;
    }

    LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
        target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelJITDefault);

    *rt = (NkLlvmRuntime_T){
        .jit = jit,
        .tsc = tsc,
        .triple = triple,
        .tm = tm,
    };
}

void nk_llvm_freeRuntime(NkLlvmRuntime rt) {
    if (rt) {
        LLVMDisposeTargetMachine(rt->tm);
        LLVMDisposeMessage(rt->triple);
        LLVMOrcDisposeLLJIT(rt->jit);
        LLVMOrcDisposeThreadSafeContext(rt->tsc);
    }
}

void nk_llvm_initRuntimeModule(NkLlvmRuntime rt, NkLlvmRuntimeModule mod) {
    LLVMOrcExecutionSessionRef es = LLVMOrcLLJITGetExecutionSession(rt->jit);

    LLVMOrcJITDylibRef jd;
    LLVMErrorRef err = LLVMOrcExecutionSessionCreateJITDylib(es, &jd, "main"); // TODO: Hardcoded dylib name
    if (err) {
        char *err_msg = LLVMGetErrorMessage(err);
        // TODO: Report errors properly
        NK_LOG_ERR("failed to create the dylib: %s", err_msg);
        LLVMDisposeErrorMessage(err_msg);
        _exit(1);
        return;
    }

    *mod = (NkLlvmRuntimeModule_T){
        .jd = jd,
    };
}

void nk_llvm_defineExternSymbols(
    NkArena *arena,
    NkLlvmRuntime rt,
    NkLlvmRuntimeModule mod,
    NkIrSymbolAddressArray syms) {
    LLVMOrcLLJITRef jit = rt->jit;

    LLVMOrcMaterializationUnitRef mu;
    NkDynArray(LLVMOrcCSymbolMapPair) llvm_syms = {.alloc = nk_arena_getAllocator(arena)};
    NK_ITERATE(NkIrSymbolAddress const *, it, syms) {
        nkda_append(
            &llvm_syms,
            ((LLVMOrcCSymbolMapPair){
                .Name = LLVMOrcLLJITMangleAndIntern(jit, nk_atom2cs(it->sym)),
                .Sym = {(LLVMOrcJITTargetAddress)(uintptr_t)it->addr, {0}},
            }));
    }
    mu = LLVMOrcAbsoluteSymbols(llvm_syms.data, llvm_syms.size);

    LLVMErrorRef err = LLVMOrcJITDylibDefine(mod->jd, mu);
    if (err) {
        char *err_msg = LLVMGetErrorMessage(err);
        // TODO: Report errors properly
        NK_LOG_ERR("define symbol: %s", err_msg);
        LLVMDisposeErrorMessage(err_msg);
        _exit(1);
        return;
    }
}

void nk_llvm_emitObjectFile(NkArena *arena, NkLlvmState llvm, NkIrSymbolArray syms, NkString obj_file) {
    LLVMModuleRef module = compilerIr(arena, llvm, syms);
    optimizeModule(arena, module, llvm->tm, NkLlvmOptLevel_O3); // TODO: Hardcoded opt level

    char *error = NULL;
    if (LLVMTargetMachineEmitToFile(llvm->tm, module, obj_file.data, LLVMObjectFile, &error) != 0) {
        // TODO: Report errors properly
        NK_LOG_ERR("Failed to emit object file: %s", error);
        _exit(1);
        return;
    }
}

void *nk_llvm_getSymbolAddress(
    NkArena *arena,
    NkLlvmState llvm,
    NkLlvmRuntime rt,
    NkLlvmRuntimeModule mod,
    NkIrSymbolArray syms,
    NkAtom sym) {
    LLVMOrcLLJITRef jit = rt->jit;

    LLVMModuleRef module = compilerIr(arena, llvm, syms);
    optimizeModule(arena, module, rt->tm, NkLlvmOptLevel_O3); // TODO: Hardcoded opt level

    LLVMOrcThreadSafeModuleRef tsm = LLVMOrcCreateNewThreadSafeModule(module, rt->tsc);
    LLVMErrorRef err = LLVMOrcLLJITAddLLVMIRModule(jit, mod->jd, tsm);
    if (err) {
        char *err_msg = LLVMGetErrorMessage(err);
        // TODO: Report errors properly
        NK_LOG_ERR("Failed to optimize: %s", err_msg);
        _exit(1);
        return NULL;
    }

    return lookupSymbol(jit, mod->jd, nk_atom2cs(sym));
}
