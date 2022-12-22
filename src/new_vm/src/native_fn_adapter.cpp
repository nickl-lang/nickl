#include "native_fn_adapter.h"

#include <cassert>
#include <unordered_map>

#include <ffi.h>

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/value.h"

namespace {

NK_LOG_USE_SCOPE(native_fn_adapter);

std::unordered_map<nk_typeid_t, ffi_type *> s_typemap;
NkAllocator *s_typearena;

ffi_type *_getNativeHandle(nktype_t type) {
    NK_LOG_TRC(__func__);

    static struct TypeArenaDeleter { // TODO Hack with static deleter
        ~TypeArenaDeleter() {
            nk_free_arena(s_typearena);
        }
    } s_typearena_deleter;

    if (!type) {
        return &ffi_type_void;
    }

    {
        auto it = s_typemap.find(type->id);
        if (it != s_typemap.end()) {
            return it->second;
        }
    }

    if (!s_typearena) {
        s_typearena = nk_create_arena();
    }

    ffi_type *ffi_t = nullptr;

    switch (type->typeclass_id) {
    case NkType_Array: {
        auto native_elem_h = _getNativeHandle(type->as.arr.elem_type);
        ffi_type **elements =
            (ffi_type **)nk_allocate(s_typearena, type->as.arr.elem_count * sizeof(void *));
        std::fill_n(elements, type->as.arr.elem_count, native_elem_h);
        ffi_t = (ffi_type *)nk_allocate(s_typearena, sizeof(ffi_type));
        *ffi_t = {
            .size = type->size,
            .alignment = type->alignment,
            .type = FFI_TYPE_STRUCT,
            .elements = elements,
        };
        break;
    }
    case NkType_Fn:
        assert(!"_getNativeHandle(NkType_Fn) is not implemented");
        break;
    case NkType_Numeric:
        switch (type->as.num.value_type) {
        case Int8:
            ffi_t = &ffi_type_sint8;
            break;
        case Int16:
            ffi_t = &ffi_type_sint16;
            break;
        case Int32:
            ffi_t = &ffi_type_sint32;
            break;
        case Int64:
            ffi_t = &ffi_type_sint64;
            break;
        case Uint8:
            ffi_t = &ffi_type_uint8;
            break;
        case Uint16:
            ffi_t = &ffi_type_uint16;
            break;
        case Uint32:
            ffi_t = &ffi_type_uint32;
            break;
        case Uint64:
            ffi_t = &ffi_type_uint64;
            break;
        case Float32:
            ffi_t = &ffi_type_float;
            break;
        case Float64:
            ffi_t = &ffi_type_double;
            break;
        default:
            assert(!"unreachable");
            break;
        }
        break;
    case NkType_Ptr:
        ffi_t = &ffi_type_pointer;
        break;
    case NkType_Tuple: {
        ffi_type **elements =
            (ffi_type **)nk_allocate(s_typearena, type->as.tuple.elems.size * sizeof(void *));
        for (size_t i = 0; i < type->as.tuple.elems.size; i++) {
            elements[i] = _getNativeHandle(type->as.tuple.elems.data[i].type);
        }
        ffi_t = (ffi_type *)nk_allocate(s_typearena, sizeof(ffi_type));
        *ffi_t = {
            .size = type->size,
            .alignment = type->alignment,
            .type = FFI_TYPE_STRUCT,
            .elements = elements,
        };
        break;
    }
    case NkType_Void:
        ffi_t = &ffi_type_void;
        break;
    default:
        assert(!"unreachable");
        break;
    }

    s_typemap.emplace(type->id, ffi_t);

    return ffi_t;
}

} // namespace

void nk_native_invoke(nkval_t fn, nkval_t ret, nkval_t args) {
    NK_LOG_TRC(__func__);

    size_t const argc = nkval_data(args) ? nkval_tuple_size(args) : 0;

    auto rtype = _getNativeHandle(nkval_typeof(ret));
    auto atypes = _getNativeHandle(nkval_typeof(args));

    ffi_cif cif;
    ffi_status status;
    if (nkval_typeof(fn)->as.fn.is_variadic) {
        status = ffi_prep_cif_var(
            &cif,
            FFI_DEFAULT_ABI,
            nkval_typeof(fn)->as.fn.args_t->as.tuple.elems.size,
            argc,
            rtype,
            atypes->elements);
    } else {
        status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc, rtype, atypes->elements);
    }
    assert(status == FFI_OK && "ffi_prep_cif failed");

    void **argv = (void **)nk_allocate(
        nk_default_allocator, argc * sizeof(void *)); // TODO Unnecessary allocation on every call
    defer {
        nk_free(nk_default_allocator, argv);
    };

    for (size_t i = 0; i < argc; i++) {
        argv[i] = nkval_data(nkval_tuple_at(args, i));
    }

    ffi_call(&cif, (void (*)())nkval_as(void *, fn), nkval_data(ret), argv);
}
