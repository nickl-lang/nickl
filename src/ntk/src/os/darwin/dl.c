#include "ntk/os/dl.h"

#include <dlfcn.h>

#include "common.h"

char const *nkdl_file_extension = "dylib";

NkOsHandle nkdl_loadLibrary(char const *name) {
    return handle_fromNative(dlopen(name, RTLD_LOCAL | RTLD_LAZY));
}

void nkdl_freeLibrary(NkOsHandle h_lib) {
    dlclose(handle_toNative(h_lib));
}

void *nkdl_resolveSymbol(NkOsHandle h_lib, char const *name) {
    return dlsym(handle_toNative(h_lib), name);
}

char const *nkdl_getLastErrorString(void) {
    return dlerror();
}
