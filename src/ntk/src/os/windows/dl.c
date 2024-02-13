#include "ntk/os/dl.h"

#include "common.h"
#include "ntk/os/error.h"

char const *nkdl_file_extension = "dll";

NkOsHandle nkdl_loadLibrary(char const *name) {
    return handle_fromNative((name && name[0]) ? LoadLibrary(name) : GetModuleHandle(NULL));
}

void nkdl_freeLibrary(NkOsHandle h_lib) {
    FreeLibrary(handle_toNative(h_lib));
}

void *nkdl_resolveSymbol(NkOsHandle h_lib, char const *sym) {
    return (void *)(intptr_t)GetProcAddress(handle_toNative(h_lib), sym);
}

char const *nkdl_getLastErrorString(void) {
    return nk_getLastErrorString();
}
