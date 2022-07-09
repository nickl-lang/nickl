#ifndef HEADER_GUARD_NKL_LANG_VALUE
#define HEADER_GUARD_NKL_LANG_VALUE

#include "nk/str/string_builder.hpp"
#include "nk/utils/id.hpp"
#include "nk/vm/value.hpp"

namespace nkl {

using namespace nk;

using vm::type_t;
using vm::typeclassid_t;
using vm::typeid_t;
using vm::value_t;

enum EExtTypeclassId {
    Type_Struct = vm::Typeclass_Count,

    Typeclass_Count,
};

struct _type_struct {
    Slice<Id const> field_names;
};

struct Type : vm::Type {
    union {
        _type_struct strukt;
    } as;
};

struct Field {
    Id name;
    type_t type;
    value_t init;
};

struct types : vm::types {
    static type_t get_struct(Slice<Field const> fields, size_t decl_id = 0);

    static StringBuilder &inspect(type_t type, StringBuilder &sb);

    static size_t struct_size(type_t self);
    static type_t struct_typeAt(type_t self, size_t i);
    static size_t struct_offsetAt(type_t self, size_t i);
    static Id struct_nameAt(type_t self, size_t i);
    static value_t struct_initAt(type_t self, size_t i);

    static Type *ext(type_t type) {
        return (Type *)type;
    }
};

size_t val_struct_size(value_t self);
value_t val_struct_at(value_t self, Id name);

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_VALUE
