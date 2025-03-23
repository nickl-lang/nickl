#include "ntk/dl.h"

#include <dlfcn.h>

#include "common.h"

NkHandle nkdl_loadLibrary(char const *name) {
    return handle_fromNative(dlopen((name && name[0]) ? name : NULL, RTLD_LOCAL | RTLD_LAZY));
}

void nkdl_freeLibrary(NkHandle h_lib) {
    dlclose(handle_toNative(h_lib));
}

void *nkdl_resolveSymbol(NkHandle h_lib, char const *name) {
    return dlsym(handle_toNative(h_lib), name);
}

char const *nkdl_getLastErrorString(void) {
    return dlerror();
}
