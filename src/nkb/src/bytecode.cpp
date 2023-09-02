#include "bytecode.hpp"

#include "ir_impl.hpp"
#include "nk/common/logger.h"

namespace {

NK_LOG_USE_SCOPE(bytecode);

} // namespace

char const *nkbcOpcodeName(uint16_t code) {
    return "TODO nkbcOpcodeName not implemented";
}

NkIrRunCtx nkir_createRunCtx(NkIrProg ir) {
    NK_LOG_TRC("%s", __func__);

    return new (nk_alloc_t<NkIrRunCtx_T>(ir->alloc)) NkIrRunCtx_T{
        .ir = ir,

        .procs = decltype(NkIrRunCtx_T::procs)::create(ir->alloc),
        .instrs = decltype(NkIrRunCtx_T::instrs)::create(ir->alloc),
    };
}

void nkir_freeRunCtx(NkIrRunCtx ctx) {
    NK_LOG_TRC("%s", __func__);

    nk_free_t(ctx->ir->alloc, ctx);
}

void nkir_invoke(NkIrRunCtx ctx, NkIrProc proc, NkIrPtrArray args, NkIrPtrArray ret) { // TODO
    NK_LOG_TRC("%s", __func__);

    puts("Hello, World!");
    **(int **)ret.data = 0;
}
