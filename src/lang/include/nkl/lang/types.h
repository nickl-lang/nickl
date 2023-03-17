#ifndef HEADER_GUARD_NKL_LANG_TYPES
#define HEADER_GUARD_NKL_LANG_TYPES

#include <stdint.h>

#include "nk/vm/value.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NklType_Slice = NkTypeclass_Count,

    NklTypeclass_Count,
} NklTypeclassId;

nktype_t nkl_get_array(nktype_t elem_type, size_t elem_count);
nktype_t nkl_get_fn(NktFnInfo info);
nktype_t nkl_get_numeric(NkNumericValueType value_type);
nktype_t nkl_get_ptr(nktype_t target_type);
nktype_t nkl_get_tuple(NkAllocator alloc, nktype_t const *types, size_t count, size_t stride);
nktype_t nkl_get_void();

nktype_t nkl_get_slice(NkAllocator alloc, nktype_t elem_type);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_TYPES
