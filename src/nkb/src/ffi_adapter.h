#ifndef NKB_FFI_ADAPTER_H_
#define NKB_FFI_ADAPTER_H_

#include "bytecode.h"
#include "nkb/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    union {
        void *native;
        NkBcProc bytecode;
    } proc;
    usize nfixedargs;
    bool is_variadic;
    void **argv;
    nktype_t const *argt;
    usize argc;
    void *retv;
    nktype_t rett;
} NkNativeCallData;

void nk_native_invoke(NkFfiContext *ctx, NkArena *stack, NkNativeCallData const *call_data);
void *nk_native_makeClosure(NkFfiContext *ctx, NkArena *stack, NkAllocator alloc, NkNativeCallData const *call_data);

#ifdef __cplusplus
}
#endif

#endif // NKB_FFI_ADAPTER_H_
