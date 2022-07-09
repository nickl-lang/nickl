#ifndef HEADER_GUARD_NKL_LANG_VALUE
#define HEADER_GUARD_NKL_LANG_VALUE

#include "nk/str/string_builder.hpp"
#include "nk/utils/id.hpp"
#include "nk/vm/value.hpp"

namespace nkl {

using namespace nk;

using vm::typeclassid_t;
using vm::typeid_t;
using vm::value_t;

enum EExtTypeclassId {
    Type_Struct = vm::Typeclass_Count,

    Typeclass_Count,
};

struct Type;
using type_t = Type const *;

struct _type_struct {
    Slice<Id const> field_names;
};

struct Type : vm::Type {
    union {
        _type_struct strukt;
    } as_ext;
};

struct Field {
    Id name;
    vm::type_t type;
    vm::value_t init;
};

type_t type_get_struct(Slice<Field const> fields, size_t decl_id = 0);

StringBuilder &type_name(type_t type, StringBuilder &sb);

size_t val_struct_size(value_t self);
value_t val_struct_at(value_t self, Id name);

size_t type_struct_size(type_t self);
type_t type_struct_typeAt(type_t self, size_t i);
Id type_struct_nameAt(type_t self, size_t i);
value_t type_struct_initAt(type_t self, size_t i);

inline type_t val_typeof(value_t val) {
    return (type_t)vm::val_typeof(val);
}

} // namespace nkl

#endif // HEADER_GUARD_NKL_LANG_VALUE
