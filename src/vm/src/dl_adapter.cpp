#include "dl_adapter.h"

#include <dlfcn.h>

#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string.h"

namespace {

NK_LOG_USE_SCOPE(dl_adapter);

} // namespace

NkDlHandle nkdl_open(NkString name) {
    NK_PROF_FUNC();

    auto dl = (NkDlHandle)dlopen(name.size ? name.data : nullptr, RTLD_GLOBAL | RTLD_LAZY);
    if (!dl) {
        NK_LOG_ERR("error: %s\n", dlerror()); // TODO Report errors properly
    }
    NK_LOG_DBG("dlopen(\"" NKS_FMT "\") -> %p", NKS_ARG(name), (void *)dl);
    return dl;
}

void nkdl_close(NkDlHandle dl) {
    NK_PROF_FUNC();

    if (dl) {
        dlclose((void *)dl);
    }
}

void *nkdl_sym(NkDlHandle dl, NkString sym) {
    NK_PROF_FUNC();

    auto ptr = dlsym((void *)dl, sym.data);
    if (!ptr) {
        NK_LOG_ERR("error: %s\n", dlerror()); // TODO Report errors properly
    }
    NK_LOG_DBG("dlsym(\"" NKS_FMT "\") -> %p", NKS_ARG(sym), (void *)ptr);
    return ptr;
}
