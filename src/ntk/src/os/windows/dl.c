#include "ntk/os/dl.h"

#include "ntk/os/error.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

char const *nkdl_file_extension = "dll";

NkOsHandle nkdl_loadLibrary(char const *name) {
    return (name && name[0]) ? LoadLibrary(name) : GetModuleHandle(NULL);
}

void nkdl_freeLibrary(NkOsHandle dl) {
    FreeLibrary(dl);
}

void *nkdl_resolveSymbol(NkOsHandle dl, char const *sym) {
    return (void *)(intptr_t)GetProcAddress(dl, sym);
}

char const *nkdl_getLastErrorString(void) {
    return nk_getLastErrorString();
}
