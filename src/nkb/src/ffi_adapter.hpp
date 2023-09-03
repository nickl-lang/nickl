#ifndef HEADER_GUARD_NKB_NATIVE_ADAPTER
#define HEADER_GUARD_NKB_NATIVE_ADAPTER

#include "nkb/ir.h"

void nk_native_invoke(void *proc, NkIrProcInfo const *proc_info, void **argv, size_t argc, void *ret);

#endif // HEADER_GUARD_NKB_NATIVE_ADAPTER
