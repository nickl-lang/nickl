#include "native_fn_adapter.hpp"

#include <algorithm>
#include <cassert>

#include <ffi.h>

#include "nk/mem/mem.h"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nk/utils/utils.hpp"
#include "nk/vm/value.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::native_fn_adapter);

ffi_type *_getNativeHandle(Allocator &allocator, type_t type) {
    EASY_FUNCTION(profiler::colors::Green200)
    LOG_TRC(__func__);

    if (!type) {
        return &ffi_type_void;
    }

    if (type->native_handle) {
        return (ffi_type *)type->native_handle;
    }

    ffi_type *ffi_t = nullptr;

    switch (type->typeclass_id) {
    case Type_Array: {
        auto native_elem_h = _getNativeHandle(allocator, types::array_elemType(type));
        ffi_type **elements = allocator.alloc<ffi_type *>(types::array_size(type));
        std::fill_n(elements, types::array_size(type), native_elem_h);
        ffi_t = allocator.alloc<ffi_type>();
        *ffi_t = {
            .size = type->size,
            .alignment = type->alignment,
            .type = FFI_TYPE_STRUCT,
            .elements = elements,
        };
        break;
    }
    case Type_Fn:
        assert(!"_getNativeHandle(Type_Fn) is not implemented");
        break;
    case Type_Numeric:
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
    case Type_Ptr:
        ffi_t = &ffi_type_pointer;
        break;
    case Type_Tuple: {
        ffi_type **elements = allocator.alloc<ffi_type *>(types::tuple_size(type));
        for (size_t i = 0; i < types::tuple_size(type); i++) {
            elements[i] = _getNativeHandle(allocator, types::tuple_typeAt(type, i));
        }
        ffi_t = allocator.alloc<ffi_type>();
        *ffi_t = {
            .size = type->size,
            .alignment = type->alignment,
            .type = FFI_TYPE_STRUCT,
            .elements = elements,
        };
        break;
    }
    case Type_Void:
        ffi_t = &ffi_type_void;
        break;
    default:
        assert(!"unreachable");
        break;
    }

    type->native_handle = ffi_t;

    return ffi_t;
}

struct NativeFnInfo {
    Allocator *type_allocator;
    void *body_ptr;
    size_t argc;
    bool is_variadic;
};

void _invoke_native(type_t self, value_t ret, value_t args) {
    auto &info = *(NativeFnInfo *)self->as.fn.closure;

    size_t const argc = val_data(args) ? val_tuple_size(args) : 0;

    auto rtype = _getNativeHandle(*info.type_allocator, val_typeof(ret));
    auto atypes = _getNativeHandle(*info.type_allocator, val_typeof(args));

    ffi_cif cif;
    ffi_status status;
    if (info.is_variadic) {
        status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, info.argc, argc, rtype, atypes->elements);
    } else {
        status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc, rtype, atypes->elements);
    }
    assert(status == FFI_OK && "ffi_prep_cif failed");

    void **argv = (void **)nk_platform_alloc(argc * sizeof(void *));
    defer {
        nk_platform_free(argv);
    };

    for (size_t i = 0; i < argc; i++) {
        argv[i] = val_data(val_tuple_at(args, i));
    }

    ffi_call(&cif, FFI_FN(info.body_ptr), val_data(ret), argv);
}

} // namespace

void type_fn_prepareNativeInfo(
    Allocator &allocator,
    void *body_ptr,
    size_t argc,
    bool is_variadic,
    FuncPtr &out_body,
    void *&out_closure) {
    auto &info = *allocator.alloc<NativeFnInfo>() = {
        .type_allocator = &allocator,
        .body_ptr = body_ptr,
        .argc = argc,
        .is_variadic = is_variadic,
    };

    out_body = _invoke_native;
    out_closure = &info;
}

} // namespace vm
} // namespace nk
