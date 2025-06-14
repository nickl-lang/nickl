#include "llvm_adapter.h"

#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Types.h>
#include <unistd.h> // TODO: Remove, only used for _exit

#include "llvm_adapter_internal.h"
#include "llvm_emitter.h"
#include "ntk/arena.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(llvm_adapter);

NkLlvmState nk_llvm_createState(NkArena *arena) {
    NK_LOG_TRC("%s", __func__);

    NkLlvmState llvm;
    NK_PROF_FUNC() {
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();

        llvm = nk_arena_allocT(arena, NkLlvmState_T);
        *llvm = (NkLlvmState_T){
            .arena = arena,
            .ctx = LLVMContextCreate(),
        };
    }
    return llvm;
}

void nk_llvm_freeState(NkLlvmState llvm) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC() {
        LLVMContextDispose(llvm->ctx);
    }
}

NkLlvmJitState nk_llvm_createJitState(NkLlvmState llvm) {
    NK_LOG_TRC("%s", __func__);

    NkLlvmJitState jit;
    NK_PROF_FUNC() {
        LLVMOrcThreadSafeContextRef tsc = LLVMOrcCreateNewThreadSafeContext();

        LLVMOrcLLJITBuilderRef builder = LLVMOrcCreateLLJITBuilder();

        LLVMOrcLLJITRef lljit = NULL;
        LLVMErrorRef err = LLVMOrcCreateLLJIT(&lljit, builder);
        if (err) {
            char *err_msg = LLVMGetErrorMessage(err);
            NK_LOG_ERR("Failed to create jit: %s", err_msg);
            LLVMDisposeErrorMessage(err_msg);
        }

        char *triple = LLVMGetDefaultTargetTriple();
        NkLlvmTarget tgt = nk_llvm_createTarget(llvm, triple);
        LLVMDisposeMessage(triple);

        jit = nk_arena_allocT(llvm->arena, NkLlvmJitState_T);
        *jit = (NkLlvmJitState_T){
            .lljit = lljit,
            .tsc = tsc,
            .tgt = tgt,
        };
    }
    return jit;
}

void nk_llvm_freeJitState(NkLlvmJitState jit) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC() {
        if (jit) {
            nk_llvm_freeTarget(jit->tgt);
            LLVMOrcDisposeThreadSafeContext(jit->tsc);
            LLVMOrcDisposeLLJIT(jit->lljit);
        }
    }
}

NkLlvmTarget nk_llvm_getJitTarget(NkLlvmJitState jit) {
    return jit->tgt;
}

NkLlvmJitDylib nk_llvm_createJitDylib(NkLlvmState NK_UNUSED llvm, NkLlvmJitState jit) {
    NK_LOG_TRC("%s", __func__);

    LLVMOrcJITDylibRef jd = NULL;
    NK_PROF_FUNC() {
        LLVMOrcExecutionSessionRef es = LLVMOrcLLJITGetExecutionSession(jit->lljit);

        LLVMErrorRef err = LLVMOrcExecutionSessionCreateJITDylib(es, &jd, "main");
        if (err) {
            char *err_msg = LLVMGetErrorMessage(err);
            // TODO: Report errors properly
            NK_LOG_ERR("failed to create the dylib: %s", err_msg);
            LLVMDisposeErrorMessage(err_msg);
            _exit(1);
        }
    }
    return (NkLlvmJitDylib)jd;
}

NkLlvmTarget nk_llvm_createTarget(NkLlvmState NK_UNUSED llvm, char const *triple) {
    NK_LOG_TRC("%s", __func__);

    NK_LOG_INF("Creating target for triple: %s", triple);

    LLVMTargetMachineRef tm = NULL;
    NK_PROF_FUNC() {
        char *error = NULL;

        char *triple_norm = LLVMNormalizeTargetTriple(triple);
        LLVMTargetRef t = NULL;
        if (LLVMGetTargetFromTriple(triple_norm, &t, &error)) {
            // TODO: Report errors properly
            NK_LOG_ERR("Failed to get target from triple: %s", error);
            _exit(1);
        }
        tm = LLVMCreateTargetMachine(
            t, triple_norm, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocPIC, LLVMCodeModelDefault);

        LLVMDisposeMessage(triple_norm);
    }
    return (NkLlvmTarget)tm;
}

void nk_llvm_freeTarget(NkLlvmTarget tgt) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC() {
        LLVMTargetMachineRef tm = (LLVMTargetMachineRef)tgt;
        LLVMDisposeTargetMachine(tm);
    }
}

NkLlvmModule nk_llvm_compilerIr(NkArena *scratch, NkLlvmState llvm, NkIrSymbolArray ir) {
    NK_LOG_TRC("%s", __func__);

    LLVMModuleRef module = NULL;
    NK_PROF_FUNC() {
        NkStringBuilder llvm_ir = {.alloc = nk_arena_getAllocator(scratch)};
        nk_llvm_emitIr(nksb_getStream(&llvm_ir), scratch, ir);
        nksb_appendNull(&llvm_ir);
        llvm_ir.size--;

        NK_LOG_STREAM_INF {
            NkStream log = nk_log_getStream();
            nk_printf(log, "LLVM IR:\n" NKS_FMT, NKS_ARG(llvm_ir));
        }

        LLVMMemoryBufferRef buffer = LLVMCreateMemoryBufferWithMemoryRange(llvm_ir.data, llvm_ir.size, "main", 1);

        char *error = NULL;
        if (LLVMParseIRInContext(llvm->ctx, buffer, &module, &error)) {
            // TODO: Report errors properly
            NK_LOG_ERR("Error parsing IR: %s", error);
            _exit(1);
        }
    }
    return (NkLlvmModule)module;
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

void nk_llvm_optimizeIr(NkArena *scratch, NkLlvmModule mod, NkLlvmTarget tgt, NkLlvmOptLevel opt) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC() {
        LLVMModuleRef module = (LLVMModuleRef)mod;
        LLVMTargetMachineRef tm = (LLVMTargetMachineRef)tgt;

        LLVMPassBuilderOptionsRef pbo = LLVMCreatePassBuilderOptions();

        LLVMErrorRef err = LLVMRunPasses(module, nk_tprintf(scratch, "default<O%c>", optLevelChar(opt)), tm, pbo);
        if (err) {
            char *err_msg = LLVMGetErrorMessage(err);
            // TODO: Report errors properly
            NK_LOG_ERR("Failed to optimize: %s", err_msg);
            LLVMDisposeErrorMessage(err_msg);
            _exit(1);
        }

        NK_LOG_STREAM_INF {
            char *str = LLVMPrintModuleToString(module);
            NkStream log = nk_log_getStream();
            nk_printf(log, "Optimized LLVM IR:\n%s", str);
            LLVMDisposeMessage(str);
        }

        LLVMDisposePassBuilderOptions(pbo);
    }
}

void nk_llvm_defineExternSymbols(NkArena *scratch, NkLlvmJitState jit, NkLlvmJitDylib dl, NkIrSymbolAddressArray syms) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC() {
        LLVMOrcLLJITRef lljit = jit->lljit;

        LLVMOrcMaterializationUnitRef mu = NULL;
        NkDynArray(LLVMOrcCSymbolMapPair) llvm_syms = {.alloc = nk_arena_getAllocator(scratch)};
        NK_ITERATE(NkIrSymbolAddress const *, it, syms) {
            nkda_append(
                &llvm_syms,
                ((LLVMOrcCSymbolMapPair){
                    .Name = LLVMOrcLLJITMangleAndIntern(lljit, nk_atom2cs(it->sym)),
                    .Sym = {(LLVMOrcJITTargetAddress)(uintptr_t)it->addr, {0}},
                }));
        }
        mu = LLVMOrcAbsoluteSymbols(llvm_syms.data, llvm_syms.size);

        LLVMOrcJITDylibRef jd = (LLVMOrcJITDylibRef)dl;

        LLVMErrorRef err = LLVMOrcJITDylibDefine(jd, mu);
        if (err) {
            char *err_msg = LLVMGetErrorMessage(err);
            // TODO: Report errors properly
            NK_LOG_ERR("define symbol: %s", err_msg);
            LLVMDisposeErrorMessage(err_msg);
            _exit(1);
        }
    }
}

void nk_llvm_emitObjectFile(NkLlvmModule mod, NkLlvmTarget tgt, NkString obj_file) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC() {
        LLVMTargetMachineRef tm = (LLVMTargetMachineRef)tgt;

        char *error = NULL;
        if (LLVMTargetMachineEmitToFile(tm, (LLVMModuleRef)mod, obj_file.data, LLVMObjectFile, &error) != 0) {
            // TODO: Report errors properly
            NK_LOG_ERR("Failed to emit object file: %s", error);
            _exit(1);
        }
    }
}

void nk_llvm_jitModule(NkLlvmModule mod, NkLlvmJitState jit, NkLlvmJitDylib dl) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC() {
        LLVMOrcThreadSafeModuleRef tsm = LLVMOrcCreateNewThreadSafeModule((LLVMModuleRef)mod, jit->tsc);

        LLVMOrcJITDylibRef jd = (LLVMOrcJITDylibRef)dl;

        LLVMErrorRef err = LLVMOrcLLJITAddLLVMIRModule(jit->lljit, jd, tsm);
        if (err) {
            char *err_msg = LLVMGetErrorMessage(err);
            // TODO: Report errors properly
            NK_LOG_ERR("Failed to optimize: %s", err_msg);
            LLVMDisposeErrorMessage(err_msg);
            _exit(1);
        }
    }
}

void *nk_llvm_getSymbolAddress(NkLlvmJitState jit, NkLlvmJitDylib dl, NkAtom sym) {
    NK_LOG_TRC("%s", __func__);

    void *addr = NULL;
    NK_PROF_FUNC() {
        LLVMOrcJITDylibRef jd = (LLVMOrcJITDylibRef)dl;
        addr = lookupSymbol(jit->lljit, jd, nk_atom2cs(sym));
    }
    return addr;
}
