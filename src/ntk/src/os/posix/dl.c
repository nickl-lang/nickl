#include "ntk/dl.h"

#include <dlfcn.h>

#include "common.h"

NkHandle nkdl_loadLibrary(char const *name) {
    return native2handle(dlopen((name && name[0]) ? name : NULL, RTLD_LOCAL | RTLD_LAZY));
}

void nkdl_freeLibrary(NkHandle h_lib) {
    dlclose(handle2native(h_lib));
}

void *nkdl_resolveSymbol(NkHandle h_lib, char const *name) {
    return dlsym(handle2native(h_lib), name);
}

char const *nkdl_getLastErrorString(void) {
    return dlerror();
}
