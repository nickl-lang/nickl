#ifndef HEADER_GUARD_NKL_LANG_TYPES
#define HEADER_GUARD_NKL_LANG_TYPES

#include <stdint.h>

#include "nk/common/common.h"
#include "nk/vm/common.h"
#include "nk/vm/value.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NklType_Slice = NkTypeclass_Count,

    NklTypeclass_Count,
} NklTypeclassId;

typedef struct NklType const *nkltype_t;

typedef uint8_t nkl_typeclassid_t;

typedef struct {
    nkltype_t target_type;
    bool is_const;
} _nkl_type_slice;

typedef struct NklType {
    nktype_t vm_type;
    union {
        _nkl_type_slice slice;
    } as;
} NklType;

typedef struct {
    void *data;
    nkltype_t type;
} nklval_t;

inline nklval_t nklval_undefined() {
    return nklval_t{};
}

inline void *nklval_data(nklval_t val) {
    return val.data;
}

inline nkltype_t nklval_typeof(nklval_t val) {
    return val.type;
}

inline nkl_typeclassid_t nklt_typeclassid(nkltype_t type) {
    return type->vm_type->tclass;
}

inline size_t nklt_sizeof(nkltype_t type) {
    return type->vm_type->size;
}

inline size_t nklt_alignof(nkltype_t type) {
    return type->vm_type->align;
}

inline nkl_typeclassid_t nklval_typeclassid(nklval_t val) {
    return nklt_typeclassid(nklval_typeof(val));
}

inline size_t nklval_sizeof(nklval_t val) {
    return nklt_sizeof(nklval_typeof(val));
}

inline size_t nklval_alignof(nklval_t val) {
    return nklt_alignof(nklval_typeof(val));
}

inline nklval_t nklval_reinterpret_cast(nkltype_t type, nklval_t val) {
    return nklval_t{nklval_data(val), type};
}

NK_EXPORT void nkl_types_clean();

NK_EXPORT nktype_t nkl_get_array(nktype_t elem_type, size_t elem_count);
NK_EXPORT nktype_t nkl_get_fn(NktFnInfo info);
NK_EXPORT nktype_t nkl_get_numeric(NkNumericValueType value_type);
NK_EXPORT nktype_t nkl_get_ptr(nktype_t target_type);
NK_EXPORT nktype_t
nkl_get_tuple(NkAllocator alloc, nktype_t const *types, size_t count, size_t stride);
NK_EXPORT nktype_t nkl_get_void();

NK_EXPORT nktype_t nkl_get_slice(NkAllocator alloc, nktype_t elem_type, bool is_const = false);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_LANG_TYPES
