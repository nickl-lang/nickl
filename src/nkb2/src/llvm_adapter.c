#include "llvm_adapter.h"

#include <unistd.h> // TODO: Remove

#include "llvm_emitter.h"
#include "ntk/log.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(llvm_adapter);

LLVMModuleRef nk_llvm_createModule(NkArena *scratch1, NkArena *scratch2, NkIrSymbolArray ir, LLVMContextRef ctx) {
    LLVMModuleRef module = NULL;
    NK_ARENA_SCOPE(scratch1) {
        // TODO: Emit ir selectively
        NkStringBuilder llvm_ir = {.alloc = nk_arena_getAllocator(scratch1)};
        NK_ARENA_SCOPE(scratch2) {
            nk_llvm_emitIr(nksb_getStream(&llvm_ir), scratch2, ir);
        }
        nksb_appendNull(&llvm_ir);

        NK_LOG_STREAM_INF {
            NkStream log = nk_log_getStream();
            nk_printf(log, "LLVM IR:\n" NKS_FMT, NKS_ARG(llvm_ir));
        }

        LLVMMemoryBufferRef buffer = LLVMCreateMemoryBufferWithMemoryRange(llvm_ir.data, llvm_ir.size, "buffer", 0);

        char *error = NULL;
        if (LLVMParseIRInContext(ctx, buffer, &module, &error)) {
            // TODO: Report errors properly
            NK_LOG_ERR("Error parsing IR: %s", error);
            _exit(1);
            return NULL;
        }
    }
    return module;
}

bool nk_llvm_optimize(LLVMModuleRef module, LLVMTargetMachineRef tm, LLVMPassBuilderOptionsRef pbo) {
    LLVMErrorRef err = NULL;
    if ((err = LLVMRunPasses(module, "default<O3>", tm, pbo))) { // TODO: Hardcoded opt level
        char *err_msg = LLVMGetErrorMessage(err);
        NK_LOG_ERR("Failed to optimize: %s", err_msg);
        return false;
    }
    return true;
}
