#include "nkl/lang/value.hpp"

#include "nk/mem/mem.h"
#include "nk/mem/stack_allocator.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nk/utils/utils.hpp"

namespace nkl {

namespace {

LOG_USE_SCOPE(nkl::lang::value);

} // namespace

type_t types::get_struct(Slice<Field const> fields, size_t decl_id) {
    EASY_FUNCTION(profiler::colors::Green200)

    struct Fp {
        vm::FpBase base;
        size_t decl_id;
        size_t type_count;
    };
    //@Todo Field init values ignored in type_get_struct
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
    vm::TypeQueryRes res = types::getType({(uint8_t *)fp, fp_size});
    if (res.inserted) {
        //@Performance Effectively contructing Fp twice for type_get_struct
        auto const tuple_t =
            types::get_tuple({&fields[0].type, fields.size}, sizeof(fields[0]) / sizeof(void *));

        //@Robustness Have to manually restore id and typeclass_id, because they get overwritten
        typeid_t actual_id = res.type->id;
        typeclassid_t actual_typeclassid = res.type->typeclass_id;
        *res.type = *tuple_t;
        res.type->typeclass_id = actual_typeclassid;
        res.type->id = actual_id;

        //@Todo Meamory leak with a static arena
        static StackAllocator s_arena{};
        //@Todo Should have a copy with stride method for slices
        auto field_names = s_arena.alloc<Id>(fields.size);
        size_t i = 0;
        for (auto const &field : fields) {
            field_names[i++] = field.name;
        }

        Type *res_type = (Type *)res.type;
        types::ext(res_type)->as.strukt.field_names = field_names;
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

value_t types::struct_initAt(type_t self, size_t i) {
    //@Todo types::struct_initAt not implemented
}

} // namespace nkl
