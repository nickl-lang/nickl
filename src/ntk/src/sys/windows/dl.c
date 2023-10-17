#include "ntk/sys/dl.h"

#include "ntk/sys/error.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

nkdl_t nk_load_library(char const *name) {
    return (name && name[0]) ? LoadLibrary(name) : GetModuleHandle(NULL);
}

void nk_free_library(nkdl_t dl) {
    FreeLibrary(dl);
}

void *nk_resolve_symbol(nkdl_t dl, char const *sym) {
    return (void *)GetProcAddress(dl, sym);
}

char const *nkdl_getLastErrorString() {
    return nk_getLastErrorString();
}
