#include "llvm_adapter.h"

#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <unistd.h> // TODO: Remove, only used for _exit

#include "llvm_adapter_internal.h"
#include "llvm_emitter.h"
#include "ntk/log.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(llvm_adapter);

static LLVMModuleRef compilerIr(NkLlvmState llvm, NkArena (*scratch)[2], NkIrSymbolArray ir) {
    NkArena *scratch0 = &(*scratch)[0];
    NkArena *scratch1 = &(*scratch)[1];

    LLVMModuleRef module = NULL;
    NK_ARENA_SCOPE(scratch0) {
        // TODO: Emit ir selectively
        NkStringBuilder llvm_ir = {.alloc = nk_arena_getAllocator(scratch0)};
        NK_ARENA_SCOPE(scratch1) {
            nk_llvm_emitIr(nksb_getStream(&llvm_ir), scratch1, ir);
        }
        nksb_appendNull(&llvm_ir);

        NK_LOG_STREAM_INF {
            NkStream log = nk_log_getStream();
            nk_printf(log, "LLVM IR:\n" NKS_FMT, NKS_ARG(llvm_ir));
        }

        LLVMMemoryBufferRef buffer =
            LLVMCreateMemoryBufferWithMemoryRange(llvm_ir.data, llvm_ir.size, "main", 0); // TODO: hardcoded buffer name

        char *error = NULL;
        if (LLVMParseIRInContext(llvm->ctx, buffer, &module, &error)) {
            // TODO: Report errors properly
            NK_LOG_ERR("Error parsing IR: %s", error);
            _exit(1);
            return NULL;
        }
    }
    return module;
}

static void optimizeModule(LLVMModuleRef module, LLVMTargetMachineRef tm) {
    LLVMPassBuilderOptionsRef pbo = LLVMCreatePassBuilderOptions();

    LLVMErrorRef err = NULL;
    if ((err = LLVMRunPasses(module, "default<O3>", tm, pbo))) { // TODO: Hardcoded opt level
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

void nk_llvm_init(NkLlvmState llvm) {
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();

    char *error = NULL;

    char *triple = LLVMGetDefaultTargetTriple();
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
        // TODO: Report errors properly
        NK_LOG_ERR("Failed to get target from triple: %s", error);
        _exit(1);
        return;
    }
    LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
        target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocPIC, LLVMCodeModelDefault);

    *llvm = (NkLlvmState_T){
        .ctx = LLVMContextCreate(),
        .triple = triple,
        .tm = tm,
    };
}

void nk_llvm_free(NkLlvmState llvm) {
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
    NkArena *scratch,
    NkLlvmRuntime rt,
    NkLlvmRuntimeModule mod,
    NkIrSymbolAddressArray syms) {
    LLVMOrcLLJITRef jit = rt->jit;

    LLVMOrcMaterializationUnitRef mu;
    NK_ARENA_SCOPE(scratch) {
        NkDynArray(LLVMOrcCSymbolMapPair) llvm_syms = {.alloc = nk_arena_getAllocator(scratch)};
        NK_ITERATE(NkIrSymbolAddress const *, it, syms) {
            nkda_append(
                &llvm_syms,
                ((LLVMOrcCSymbolMapPair){
                    .Name = LLVMOrcLLJITMangleAndIntern(jit, nk_atom2cs(it->sym)),
                    .Sym = {(LLVMOrcJITTargetAddress)(uintptr_t)it->addr, {0}},
                }));
        }
        mu = LLVMOrcAbsoluteSymbols(llvm_syms.data, llvm_syms.size);
    }

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

void nk_llvm_emitObjectFile(NkArena (*scratch)[2], NkLlvmState llvm, NkIrSymbolArray syms, NkString obj_file) {
    LLVMModuleRef module = compilerIr(llvm, scratch, syms);
    optimizeModule(module, llvm->tm);

    char *error = NULL;
    if (LLVMTargetMachineEmitToFile(llvm->tm, module, obj_file.data, LLVMObjectFile, &error) != 0) {
        // TODO: Report errors properly
        NK_LOG_ERR("Failed to emit object file: %s", error);
        _exit(1);
        return;
    }
}

void *nk_llvm_getSymbolAddress(
    NkArena (*scratch)[2],
    NkLlvmState llvm,
    NkLlvmRuntime rt,
    NkLlvmRuntimeModule mod,
    NkIrSymbolArray syms,
    NkAtom sym) {
    LLVMOrcLLJITRef jit = rt->jit;

    LLVMModuleRef module = compilerIr(llvm, scratch, syms);
    optimizeModule(module, rt->tm);

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
