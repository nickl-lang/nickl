#include "ntk/os/dl.h"

#include <dlfcn.h>

char const *nkdl_file_extension = "so";

NkOsHandle nkdl_loadLibrary(char const *name) {
    return dlopen(name, RTLD_LOCAL | RTLD_LAZY);
}

void nkdl_freeLibrary(NkOsHandle dl) {
    dlclose(dl);
}

void *nkdl_resolveSymbol(NkOsHandle dl, char const *name) {
    return dlsym(dl, name);
}

char const *nkdl_getLastErrorString() {
    return dlerror();
}
