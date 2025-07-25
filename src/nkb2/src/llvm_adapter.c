#include "llvm_adapter.h"

#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Types.h>

#include "llvm_adapter_internal.h"
#include "llvm_emitter.h"
#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/error.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(llvm_adapter);

#define TRY(EXPR, ...)          \
    do {                        \
        if (!(EXPR)) {          \
            return __VA_ARGS__; \
        }                       \
    } while (0)

static NkLlvmTarget tm_wrap(LLVMTargetMachineRef val) {
    return (NkLlvmTarget)val;
}
static LLVMTargetMachineRef tm_unwrap(NkLlvmTarget val) {
    return (LLVMTargetMachineRef)val;
}

static NkLlvmJitDylib jd_wrap(LLVMOrcJITDylibRef val) {
    return (NkLlvmJitDylib)val;
}
static LLVMOrcJITDylibRef jd_unwrap(NkLlvmJitDylib val) {
    return (LLVMOrcJITDylibRef)val;
}

static NkLlvmModule m_wrap(LLVMModuleRef val) {
    return (NkLlvmModule)val;
}
static LLVMModuleRef m_unwrap(NkLlvmModule val) {
    return (LLVMModuleRef)val;
}

NkLlvmState nk_llvm_createState(NkArena *arena) {
    NK_LOG_TRC("%s", __func__);

    TRY(arena, NULL);

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

    TRY(llvm);

    NK_PROF_FUNC() {
        LLVMContextDispose(llvm->ctx);
    }
}

static LLVMTargetMachineRef createTargetImpl(char const *triple, LLVMCodeModel cm) {
    char *triple_norm = LLVMNormalizeTargetTriple(triple);

    LLVMTargetRef t = NULL;
    char *error = NULL;
    if (LLVMGetTargetFromTriple(triple_norm, &t, &error)) {
        nk_error_printf("Failed to create target: %s", error);
        LLVMDisposeMessage(error);
        LLVMDisposeMessage(triple_norm);
        return NULL;
    }

    LLVMTargetMachineRef tm =
        LLVMCreateTargetMachine(t, triple_norm, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocPIC, cm);

    LLVMDisposeMessage(triple_norm);

    return tm;
}

NkLlvmJitState nk_llvm_createJitState(NkLlvmState llvm) {
    NK_LOG_TRC("%s", __func__);

    TRY(llvm, NULL);

    NkLlvmJitState jit = NULL;
    NK_PROF_FUNC() {
        LLVMOrcLLJITBuilderRef builder = LLVMOrcCreateLLJITBuilder();

        LLVMOrcLLJITRef lljit = NULL;
        LLVMErrorRef err = LLVMOrcCreateLLJIT(&lljit, builder);
        if (err) {
            char *err_msg = LLVMGetErrorMessage(err);
            nk_error_printf("Failed to create JIT: %s", err_msg);
            LLVMDisposeErrorMessage(err_msg);
        } else {
            LLVMOrcThreadSafeContextRef tsc = LLVMOrcCreateNewThreadSafeContext();

            char *triple = LLVMGetDefaultTargetTriple();
            LLVMTargetMachineRef tm = createTargetImpl(triple, LLVMCodeModelJITDefault);
            if (tm) {
                jit = nk_arena_allocT(llvm->arena, NkLlvmJitState_T);
                *jit = (NkLlvmJitState_T){
                    .lljit = lljit,
                    .tsc = tsc,
                    .tm = tm,
                };
            }
            LLVMDisposeMessage(triple);
        }
    }
    return jit;
}

void nk_llvm_freeJitState(NkLlvmJitState jit) {
    NK_LOG_TRC("%s", __func__);

    TRY(jit);

    NK_PROF_FUNC() {
        if (jit) {
            LLVMDisposeTargetMachine(jit->tm);
            LLVMOrcDisposeThreadSafeContext(jit->tsc);
            LLVMOrcDisposeLLJIT(jit->lljit);
        }
    }
}

NkLlvmTarget nk_llvm_getJitTarget(NkLlvmJitState jit) {
    TRY(jit, NULL);

    return tm_wrap(jit->tm);
}

NkLlvmJitDylib nk_llvm_createJitDylib(NkLlvmState NK_UNUSED llvm, NkLlvmJitState jit) {
    NK_LOG_TRC("%s", __func__);

    TRY(llvm && jit, NULL);

    LLVMOrcJITDylibRef jd = NULL;
    NK_PROF_FUNC() {
        LLVMOrcExecutionSessionRef es = LLVMOrcLLJITGetExecutionSession(jit->lljit);

        LLVMErrorRef err = LLVMOrcExecutionSessionCreateJITDylib(es, &jd, "main");
        if (err) {
            char *err_msg = LLVMGetErrorMessage(err);
            nk_error_printf("Failed to create the JIT DyLib: %s", err_msg);
            LLVMDisposeErrorMessage(err_msg);
        }
    }
    return jd_wrap(jd);
}

NkLlvmTarget nk_llvm_createTarget(NkLlvmState NK_UNUSED llvm, char const *triple) {
    NK_LOG_TRC("%s", __func__);

    TRY(llvm && triple, NULL);

    LLVMTargetMachineRef tm = NULL;
    NK_PROF_FUNC() {
        NK_LOG_INF("Creating target for triple: %s", triple);
        tm = createTargetImpl(triple, LLVMCodeModelDefault);
    }
    return tm_wrap(tm);
}

void nk_llvm_freeTarget(NkLlvmTarget tgt) {
    NK_LOG_TRC("%s", __func__);

    TRY(tgt);

    NK_PROF_FUNC() {
        LLVMTargetMachineRef tm = tm_unwrap(tgt);
        LLVMDisposeTargetMachine(tm);
    }
}

NkLlvmModule nk_llvm_compilerIr(NkArena *scratch, NkLlvmState llvm, NkIrSymbolArray ir) {
    NK_LOG_TRC("%s", __func__);

    TRY(scratch && llvm, NULL);

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
            nk_error_printf("Failed to parse IR: %s", error);
            LLVMDisposeMessage(error);
        }
    }
    return m_wrap(module);
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

bool nk_llvm_optimizeIr(NkArena *scratch, NkLlvmModule mod, NkLlvmTarget tgt, NkLlvmOptLevel opt) {
    NK_LOG_TRC("%s", __func__);

    TRY(scratch && mod && tgt, false);

    bool ret = true;
    NK_PROF_FUNC() {
        LLVMModuleRef module = m_unwrap(mod);
        LLVMTargetMachineRef tm = tm_unwrap(tgt);

        LLVMPassBuilderOptionsRef pbo = LLVMCreatePassBuilderOptions();

        LLVMErrorRef err = LLVMRunPasses(module, nk_tprintf(scratch, "default<O%c>", optLevelChar(opt)), tm, pbo);
        if (err) {
            char *err_msg = LLVMGetErrorMessage(err);
            nk_error_printf("Failed to optimize IR module: %s", err_msg);
            LLVMDisposeErrorMessage(err_msg);
            ret = false;
        } else {
            NK_LOG_STREAM_INF {
                char *str = LLVMPrintModuleToString(module);
                NkStream log = nk_log_getStream();
                nk_printf(log, "Optimized LLVM IR:\n%s", str);
                LLVMDisposeMessage(str);
            }
        }
        LLVMDisposePassBuilderOptions(pbo);
    }
    return ret;
}

bool nk_llvm_defineExternSymbols(NkArena *scratch, NkLlvmJitState jit, NkLlvmJitDylib dl, NkIrSymbolAddressArray syms) {
    NK_LOG_TRC("%s", __func__);

    TRY(scratch && jit && dl, false);

    bool ret = true;
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

        LLVMOrcJITDylibRef jd = jd_unwrap(dl);

        LLVMErrorRef err = LLVMOrcJITDylibDefine(jd, mu);
        if (err) {
            char *err_msg = LLVMGetErrorMessage(err);
            nk_error_printf("Failed to define extern symbols: %s", err_msg);
            LLVMDisposeErrorMessage(err_msg);
            ret = false;
        }
    }

    return ret;
}

bool nk_llvm_emitObjectFile(NkLlvmModule mod, NkLlvmTarget tgt, NkString obj_file) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod && tgt, false);

    bool ret = true;
    NK_PROF_FUNC() {
        LLVMTargetMachineRef tm = tm_unwrap(tgt);

        char *error = NULL;
        if (LLVMTargetMachineEmitToFile(tm, m_unwrap(mod), obj_file.data, LLVMObjectFile, &error) != 0) {
            nk_error_printf("Failed to emit object file `" NKS_FMT "`: %s", NKS_ARG(obj_file), error);
            LLVMDisposeMessage(error);
            ret = false;
        }
    }
    return ret;
}

bool nk_llvm_jitModule(NkLlvmModule mod, NkLlvmJitState jit, NkLlvmJitDylib dl) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod && jit && dl, false);

    bool ret = true;
    NK_PROF_FUNC() {
        LLVMOrcThreadSafeModuleRef tsm = LLVMOrcCreateNewThreadSafeModule(m_unwrap(mod), jit->tsc);

        LLVMOrcJITDylibRef jd = jd_unwrap(dl);

        LLVMErrorRef err = LLVMOrcLLJITAddLLVMIRModule(jit->lljit, jd, tsm);
        if (err) {
            char *err_msg = LLVMGetErrorMessage(err);
            nk_error_printf("Failed to compile IR module: %s", err_msg);
            LLVMDisposeErrorMessage(err_msg);
            ret = false;
        }
    }
    return ret;
}

void *nk_llvm_getSymbolAddress(NkLlvmJitState jit, NkLlvmJitDylib dl, NkAtom sym) {
    NK_LOG_TRC("%s", __func__);

    TRY(jit && dl, NULL);

    void *addr = NULL;
    NK_PROF_FUNC() {
        LLVMOrcJITDylibRef jd = jd_unwrap(dl);
        addr = lookupSymbol(jit->lljit, jd, nk_atom2cs(sym));
    }
    return addr;
}
