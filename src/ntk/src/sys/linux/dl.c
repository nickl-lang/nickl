#include "ntk/sys/dl.h"

#include <dlfcn.h>

char const *nk_dl_extension = "so";

nkdl_t nk_load_library(char const *name) {
    return dlopen(name, RTLD_LOCAL | RTLD_LAZY);
}

void nk_free_library(nkdl_t dl) {
    dlclose(dl);
}

void *nk_resolve_symbol(nkdl_t dl, char const *name) {
    return dlsym(dl, name);
}

char const *nkdl_getLastErrorString() {
    return dlerror();
}
