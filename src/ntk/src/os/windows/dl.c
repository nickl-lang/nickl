#include "ntk/dl.h"

#include "common.h"
#include "ntk/error.h"

char const *nkdl_file_extension = "dll";

NkHandle nkdl_loadLibrary(char const *name) {
    return handle_fromNative((name && name[0]) ? LoadLibrary(name) : GetModuleHandle(NULL));
}

void nkdl_freeLibrary(NkHandle h_lib) {
    FreeLibrary(handle_toNative(h_lib));
}

void *nkdl_resolveSymbol(NkHandle h_lib, char const *sym) {
    return (void *)(intptr_t)GetProcAddress(handle_toNative(h_lib), sym);
}

char const *nkdl_getLastErrorString(void) {
    return nk_getLastErrorString();
}
