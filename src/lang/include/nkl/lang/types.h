#ifndef HEADER_GUARD_NKL_LANG_TYPES
#define HEADER_GUARD_NKL_LANG_TYPES

#include "nk/vm/value.h"

#ifdef __cplusplus
extern "C" {
#endif

nktype_t nkl_get_array(NkAllocator alloc, nktype_t elem_type, size_t elem_count);
nktype_t nkl_get_fn(NkAllocator alloc, NktFnInfo info);
nktype_t nkl_get_numeric(NkAllocator alloc, NkNumericValueType value_type);
nktype_t nkl_get_ptr(NkAllocator alloc, nktype_t target_type);
nktype_t nkl_get_tuple(NkAllocator alloc, nktype_t const *types, size_t count, size_t stride);
nktype_t nkl_get_void(NkAllocator alloc);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_TYPES
