#ifndef NK_VM_NATIVE_FN_ADAPTER_H_
#define NK_VM_NATIVE_FN_ADAPTER_H_

#include "nk/vm/common.h"
#include "nk/vm/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

void nk_native_invoke(nkval_t fn, nkval_t ret, nkval_t args);

NkIrNativeClosure nk_native_make_closure(NkIrFunct fn);
void nk_native_free_closure(NkIrNativeClosure closure);

void nk_native_adapterDeinit();

#ifdef __cplusplus
}
#endif

#endif // NK_VM_NATIVE_FN_ADAPTER_H_
