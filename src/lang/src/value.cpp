#include "nkl/lang/value.hpp"

#include <cstring>

#include "nk/mem/mem.h"
#include "nk/mem/stack_allocator.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nk/utils/utils.hpp"

namespace nkl {

namespace {

LOG_USE_SCOPE(nkl::lang::value);

} // namespace

type_t types::get_any() {
    EASY_FUNCTION(profiler::colors::Green200)

    vm::FpBase fp = {};
    fp.id = Type_Any;
    vm::TypeQueryRes res = getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = sizeof(value_t);
        res.type->alignment = alignof(value_t);
    }
    return res.type;
}

type_t types::get_bool() {
    EASY_FUNCTION(profiler::colors::Green200)

    vm::FpBase fp = {};
    fp.id = Type_Bool;
    vm::TypeQueryRes res = getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = sizeof(bool);
        res.type->alignment = alignof(bool);
    }
    return res.type;
}

type_t types::get_type() {
    EASY_FUNCTION(profiler::colors::Green200)

    vm::FpBase fp = {};
    fp.id = Type_Type;
    vm::TypeQueryRes res = getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = sizeof(void *);
        res.type->alignment = alignof(void *);
    }
    return res.type;
}

type_t types::get_struct(Slice<Field const> fields, size_t decl_id) {
    EASY_FUNCTION(profiler::colors::Green200)

    struct Fp {
        vm::FpBase base;
        size_t decl_id;
        size_t type_count;
    };
    struct FpField {
        Id name;
        typeid_t type;
    };
    size_t const fp_size = arrayWithHeaderSize<Fp, FpField>(fields.size);
    Fp *fp = (Fp *)nk_platform_alloc_aligned(fp_size, alignof(Fp));
    defer {
        nk_platform_free_aligned(fp);
    };
    auto fp_fields = arrayWithHeaderData<Fp, FpField>(fp);
    std::memset(fp, 0, fp_size);
    fp->base.id = Type_Struct;
    fp->type_count = fields.size;
    fp->decl_id = decl_id;
    for (size_t i = 0; i < fields.size; i++) {
        fp_fields[i] = {
            .name = fields[i].name,
            .type = fields[i].type->id,
        };
    }
    vm::TypeQueryRes res = getType({(uint8_t *)fp, fp_size});
    if (res.inserted) {
        vm::TupleLayout layout = vm::calcTupleLayout(
            {&fields[0].type, fields.size}, s_typearena, sizeof(fields[0]) / sizeof(void *));

        res.type->size = layout.size;
        res.type->alignment = layout.align;
        res.type->as.tuple.elems = layout.info_ar;

        auto field_names = s_typearena.alloc<Id>(fields.size);
        SLICE_COPY_RSTRIDE(field_names, fields, name);

        types::ext(res.type)->as.strukt.field_names = field_names;
    }
    return (type_t)res.type;
}

StringBuilder &types::inspect(type_t type, StringBuilder &sb) {
    switch (type->typeclass_id) {
    case Type_Struct:
        sb << "struct{";
        for (size_t i = 0; i < types::struct_size(type); i++) {
            if (i) {
                sb << ", ";
            }
            sb << id2s(types::struct_nameAt(type, i)) << ": ";
            types::inspect(types::struct_typeAt(type, i), sb);
        }
        sb << "}";
        break;
    default:
        vm::types::inspect(type, sb);
        break;
    }

    return sb;
}

static size_t _struct_indexAt(type_t self, Id name) {
    size_t i = 0;
    for (Id field_name : types::ext(self)->as.strukt.field_names) {
        if (field_name == name) {
            return i;
        }
        i++;
    }
    return -1;
}

size_t val_struct_size(value_t self) {
    return types::struct_size(val_typeof(self));
}

value_t val_struct_at(value_t self, Id name) {
    return val_tuple_at(self, _struct_indexAt(val_typeof(self), name));
}

size_t types::struct_size(type_t self) {
    return types::tuple_size(self);
}

type_t types::struct_typeAt(type_t self, size_t i) {
    return types::tuple_typeAt(self, i);
}

size_t types::struct_offsetAt(type_t self, size_t i) {
    return types::tuple_offsetAt(self, i);
}

Id types::struct_nameAt(type_t self, size_t i) {
    return types::ext(self)->as.strukt.field_names[i];
}

vm::TypeQueryRes types::getType(Slice<uint8_t const> fp, size_t type_size) {
    return vm::types::getType(fp, type_size);
}

} // namespace nkl
