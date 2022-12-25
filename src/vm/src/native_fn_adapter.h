#ifndef HEADER_GUARD_NK_VM_NATIVE_FN_ADAPTER
#define HEADER_GUARD_NK_VM_NATIVE_FN_ADAPTER

#include "nk/vm/common.h"

#ifdef __cplusplus
extern "C" {
#endif

void nk_native_invoke(nkval_t fn, nkval_t ret, nkval_t args);

typedef struct NkNativeClosure_T *NkNativeClosure;

NkNativeClosure nk_native_make_closure(nkval_t fn);
void nk_native_free_closure(NkNativeClosure closure);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_NATIVE_FN_ADAPTER
