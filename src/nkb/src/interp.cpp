#include "interp.hpp"

#include "nk/common/logger.h"

namespace {

NK_LOG_USE_SCOPE(interp);

} // namespace

void nkir_interp_invoke(NkBcProc proc, NkIrPtrArray args, NkIrPtrArray ret) {
    NK_LOG_TRC("%s", __func__);

    puts("Hello, World!");
    **(int **)ret.data = 0;
}
