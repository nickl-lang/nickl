#ifndef HEADER_GUARD_NKL_CORE_VALUE
#define HEADER_GUARD_NKL_CORE_VALUE

#include <cstddef>
#include <cstdint>

#include "nk/common/hashmap.hpp"
#include "nkl/core/common.hpp"
#include "nkl/core/id.hpp"

struct Type;
using type_t = Type const *;

using typeid_t = size_t;
using typeclassid_t = uint8_t;

enum ETypeclassId {
    Type_Any,
    Type_Array,
    Type_ArrayPtr,
    Type_Bool,
    Type_Fn,
    Type_FnPtr,
    Type_Numeric,
    Type_Struct,
    Type_Ptr,
    Type_String,
    Type_Symbol,
    Type_Tuple,
    Type_Typeref,
    Type_Void,
};

enum ETypeQualifier {
    TypeQual_None = 0,

    TypeQual_Const = 0x1,
    TypeQual_Volatile = 0x2,
    TypeQual_Align = 0x4,
};

struct QualifierRecord {
    uint8_t qualifiers;
    uint8_t alignment;
};

using FuncPtr = void (*)(...);

struct TypeArray {
    size_t size;
    type_t const *data;
};

struct IdArray {
    size_t size;
    Id const *data;
};

// 0x<index><size>
// Ordered in coercion order, max(lhs, rhs) is a common type
enum ENumericValueType {
    Int8 = 0x01,
    Uint8 = 0x11,
    Int16 = 0x22,
    Uint16 = 0x32,
    Int32 = 0x44,
    Uint32 = 0x54,
    Int64 = 0x68,
    Uint64 = 0x78,
    Float32 = 0x84,
    Float64 = 0x98,
};

#define NUM_TYPE_SIZE_MASK 0xf

struct TupleElemInfo {
    type_t type;
    size_t offset;
};

struct TupleElemInfoArray {
    size_t size;
    TupleElemInfo *data;
};

struct NameType {
    Id name;
    type_t type;
};

struct NameTypeid {
    Id name;
    typeid_t id;
};

struct NameTypeArray {
    size_t size;
    NameType *data;
};

struct _type_null {
    uint8_t _dummy;
};

struct _type_array {
    type_t elem_type;
    size_t elem_count;
};

struct _type_array_ptr {
    type_t elem_type;
};

struct _type_fn {
    type_t ret_type;
    TypeArray param_types;
    FuncPtr body;
    void *closure;
    bool args_passthrough;
};

struct _type_fn_ptr {
    type_t ret_type;
    TypeArray param_types;
};

struct _type_numeric {
    ENumericValueType value_type;
};

struct _type_ptr {
    type_t target_type;
};

struct _type_struct {
    TupleElemInfoArray types;
    IdArray fields;
};

struct _type_tuple {
    TupleElemInfoArray types;
};

struct Type {
    union {
        _type_null null_t;

        _type_array arr_t;
        _type_array_ptr arr_ptr_t;
        _type_fn fn_t;
        _type_fn_ptr fn_ptr_t;
        _type_numeric num_t;
        _type_ptr ptr_t;
        _type_struct struct_t;
        _type_tuple tuple_t;
    } as;
    uint64_t id;
    uint64_t size;
    uint8_t alignment;
    uint8_t typeclass_id;
    uint8_t qualifiers;
};

struct value_t {
    type_t type;
    void *data;
};

void types_init();
void types_deinit();

type_t type_get_any();
type_t type_get_array(type_t elem_type, size_t elem_count);
type_t type_get_array_ptr(type_t elem_type);
type_t type_get_bool();
type_t type_get_fn(
    Allocator *tmp_allocator,
    type_t ret_type,
    TypeArray param_types,
    size_t decl_id,
    FuncPtr body,
    void *closure,
    bool args_passthrough);
type_t type_get_fn_ptr(Allocator *tmp_allocator, type_t ret_type, TypeArray param_types);
type_t type_get_numeric(ENumericValueType value_type);
type_t type_get_ptr(type_t target_type);
type_t type_get_string();
type_t type_get_struct(Allocator *tmp_allocator, NameTypeArray fields, size_t decl_id);
type_t type_get_symbol();
type_t type_get_tuple(Allocator *tmp_allocator, TypeArray types);
type_t type_get_typeref();
type_t type_get_void();

string type_name(Allocator *allocator, type_t type);

value_t val_undefined();

void *val_data(value_t val);
type_t val_typeof(value_t val);
size_t val_sizeof(value_t val);
size_t val_alignof(value_t val);

value_t val_reinterpret_cast(type_t type, value_t val);

bool val_isTrue(value_t val);

#define val_as(type, val) (*(type *)(val).data)

#endif // HEADER_GUARD_NKL_CORE_VALUE
