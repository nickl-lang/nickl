#include "types.h"

void nkl_types_init(NkAllocator alloc) {
}

void nkl_types_free() {
}

nkltype_t nkl_get_any() {
}

nkltype_t nkl_get_array(nkltype_t elem_type, size_t elem_count) {
}

nkltype_t nkl_get_enum(NklFieldArray fields) {
}

nkltype_t nkl_get_numeric(NkIrNumericValueType value_type) {
}

nkltype_t nkl_get_proc(NklProcInfo info) {
}

nkltype_t nkl_get_ptr(nkltype_t target_type, bool is_const) {
}

nkltype_t nkl_get_slice(nkltype_t target_type, bool is_const) {
}

nkltype_t nkl_get_struct(NklFieldArray fields) {
}

nkltype_t nkl_get_struct_packed(NklFieldArray fields) {
}

nkltype_t nkl_get_tuple(NklTypeArray types) {
}

nkltype_t nkl_get_tuple_packed(NklTypeArray types) {
}

nkltype_t nkl_get_typeref() {
}

nkltype_t nkl_get_union(NklFieldArray fields) {
}

void nkl_type_inspect(nkltype_t type, NkStream out) {
}
