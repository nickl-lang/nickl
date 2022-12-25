#include "dl_adapter.h"

#include <dlfcn.h>

#include "nk/common/logger.h"

namespace {

NK_LOG_USE_SCOPE(dl_adapter);

} // namespace

NkDlHandle nkdl_open(nkstr name) {
    auto dl = (NkDlHandle)dlopen(name.data, RTLD_LOCAL | RTLD_LAZY);
    if (!dl) {
        NK_LOG_ERR("error: %s\n", dlerror()); // TODO Report errors properly
    }
    return dl;
}

void nkdl_close(NkDlHandle dl) {
    dlclose((void *)dl);
}

void *nkdl_sym(NkDlHandle dl, nkstr sym) {
    auto ptr = dlsym((void *)dl, sym.data);
    if (!ptr) {
        NK_LOG_ERR("error: %s\n", dlerror()); // TODO Report errors properly
    }
    return ptr;
}
