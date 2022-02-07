#include "nk/vm/private/native_fn_adapter.hpp"

#include <cassert>

#include <ffi.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/profiler.hpp"
#include "nk/vm/value.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::native_fn_adapter)

ffi_type *_getNativeHandle(Allocator &allocator, type_t type) {
    EASY_FUNCTION(profiler::colors::Green200)
    LOG_TRC(__FUNCTION__);

    if (type->native_handle) {
        return (ffi_type *)type->native_handle;
    }

    ffi_type *ffi_t = nullptr;

    switch (type->typeclass_id) {
    case Type_Array: {
        auto elem_t = type->as.arr.elem_type;
        auto elem_count = type->as.arr.elem_count;
        ffi_type **elements = allocator.alloc<ffi_type *>(elem_count);
        auto native_elem_h = _getNativeHandle(allocator, elem_t);
        for (size_t i = 0; i < elem_count; i++) {
            elements[i] = native_elem_h;
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
        auto elems = type->as.tuple.elems;
        ffi_type **elements = allocator.alloc<ffi_type *>(elems.size);
        for (size_t i = 0; i < elems.size; i++) {
            elements[i] = _getNativeHandle(allocator, elems[i].type);
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
    case Type_Typeref:
        ffi_t = &ffi_type_pointer;
        break;
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

} // namespace

struct NativeFnInfo {
    Allocator *type_allocator;
    void *body_ptr;
    size_t argc;
    bool is_variadic;
};

native_fn_info_t type_fn_prepareNativeInfo(
    Allocator &allocator,
    void *body_ptr,
    size_t argc,
    bool is_variadic) {
    auto &info = *allocator.alloc<NativeFnInfo>() = {
        .type_allocator = &allocator,
        .body_ptr = body_ptr,
        .argc = argc,
        .is_variadic = is_variadic,
    };
    return &info;
}

void val_fn_invoke_native(type_t self, value_t ret, value_t args) {
    //@Refactor Somehow allocate ffi arguments on the stack???
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto info = self->as.fn.body.native;

    size_t const argc = val_tuple_size(args);

    auto rtype = _getNativeHandle(*info->type_allocator, val_typeof(ret));
    auto atypes = _getNativeHandle(*info->type_allocator, val_typeof(args));

    ffi_cif cif;
    ffi_status status;
    if (info->is_variadic) {
        status = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, info->argc, argc, rtype, atypes->elements);
    } else {
        status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argc, rtype, atypes->elements);
    }
    if (status != FFI_OK) {
        assert("!ffi_prep_cif failed");
    }

    void **argv = tmp_arena.alloc<void *>(argc);
    for (size_t i = 0; i < argc; i++) {
        argv[i] = val_data(val_tuple_at(args, i));
    }

    ffi_call(&cif, FFI_FN(info->body_ptr), val_data(ret), argv);
}

} // namespace vm
} // namespace nk
