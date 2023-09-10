#ifndef HEADER_GUARD_NKB_FFI_ADAPTER
#define HEADER_GUARD_NKB_FFI_ADAPTER

#include "bytecode.hpp"
#include "nkb/common.h"

void nk_native_invoke(
    void *proc,
    size_t nfixedargs,
    bool is_variadic,
    void **argv,
    nktype_t const *argt,
    size_t argc,
    void *ret,
    nktype_t rett);

void *nk_native_makeClosure(
    NkAllocator alloc,
    NkBcProc proc,
    bool is_variadic,
    nktype_t const *argt,
    size_t argc,
    nktype_t rett);

#endif // HEADER_GUARD_NKB_FFI_ADAPTER
