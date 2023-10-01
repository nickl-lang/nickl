#include "dl_adapter.h"

#include <dlfcn.h>

#include "ntk/logger.h"
#include "ntk/profiler.hpp"
#include "ntk/string.h"

namespace {

NK_LOG_USE_SCOPE(dl_adapter);

} // namespace

NkDlHandle nkdl_open(nkstr name) {
    EASY_FUNCTION(::profiler::colors::Orange200);

    auto dl = (NkDlHandle)dlopen(name.size ? name.data : nullptr, RTLD_GLOBAL | RTLD_LAZY);
    if (!dl) {
        NK_LOG_ERR("error: %s\n", dlerror()); // TODO Report errors properly
    }
    NK_LOG_DBG("dlopen(\"" nkstr_Fmt "\") -> %p", nkstr_Arg(name), (void *)dl);
    return dl;
}

void nkdl_close(NkDlHandle dl) {
    EASY_FUNCTION(::profiler::colors::Orange200);

    if (dl) {
        dlclose((void *)dl);
    }
}

void *nkdl_sym(NkDlHandle dl, nkstr sym) {
    EASY_FUNCTION(::profiler::colors::Orange200);

    auto ptr = dlsym((void *)dl, sym.data);
    if (!ptr) {
        NK_LOG_ERR("error: %s\n", dlerror()); // TODO Report errors properly
    }
    NK_LOG_DBG("dlsym(\"" nkstr_Fmt "\") -> %p", nkstr_Arg(sym), (void *)ptr);
    return ptr;
}
