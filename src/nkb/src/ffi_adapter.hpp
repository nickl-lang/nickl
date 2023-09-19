#ifndef HEADER_GUARD_NKB_FFI_ADAPTER
#define HEADER_GUARD_NKB_FFI_ADAPTER

#include "bytecode.hpp"
#include "nkb/common.h"

typedef struct {
    union {
        void *native;
        NkBcProc bytecode;
    } proc;
    size_t nfixedargs;
    bool is_variadic;
    void **argv;
    nktype_t const *argt;
    size_t argc;
    void *retv;
    nktype_t rett;
} NkNativeCallData;

void nk_native_invoke(NkFfiContext *ctx, NkArena *stack, NkNativeCallData const *call_data);
void *nk_native_makeClosure(NkFfiContext *ctx, NkArena *stack, NkAllocator alloc, NkNativeCallData const *call_data);

#endif // HEADER_GUARD_NKB_FFI_ADAPTER
