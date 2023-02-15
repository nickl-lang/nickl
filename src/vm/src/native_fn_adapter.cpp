#include "native_fn_adapter.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <unordered_map>

#include <ffi.h>

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/value.h"

struct NkNativeClosure_T {
    void *code;
    ffi_cif cif;
    ffi_closure *closure;
    nkval_t fn;
};

namespace {

NK_LOG_USE_SCOPE(native_fn_adapter);

std::unordered_map<nk_typeid_t, ffi_type *> s_typemap;
NkAllocator *s_typearena;

static struct TypeArenaDeleter { // TODO Hack with static deleter
    ~TypeArenaDeleter() {
        if (s_typearena) {
            nk_free_arena(s_typearena);
        }
    }
} s_typearena_deleter;

ffi_type *_getNativeHandle(nktype_t type) {
    NK_LOG_TRC(__func__);

    if (!type) {
        NK_LOG_DBG("ffi(null) -> void");
        return &ffi_type_void;
    }

    ffi_type *ffi_t = nullptr;

    auto it = s_typemap.find(type->id);
    if (it != s_typemap.end()) {
        NK_LOG_DBG("Found existing ffi type=%p", it->second);
        ffi_t = it->second;
    } else {
        if (!s_typearena) {
            s_typearena = nk_create_arena();
        }

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
            ffi_t = &ffi_type_pointer;
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
    }

#ifdef ENABLE_LOGGING
    {
        // TODO Inspecting type in _getNativeHandle outside of the log macro
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };

        nkt_inspect(type, sb);
        auto str = nksb_concat(sb);

        NK_LOG_DBG("ffi(type{id=%llu name=%.*s}) -> %p", type->id, str.size, str.data, ffi_t);
    }
#endif // ENABLE_LOGGING

    return ffi_t;
}

void _ffiPrepareCif(ffi_cif *cif, nktype_t fn_t) { // TODO Unify ffi cif preparation
    size_t const argc = fn_t->as.fn.args_t->as.tuple.elems.size;

    auto rtype = _getNativeHandle(fn_t->as.fn.ret_t);
    auto atypes = _getNativeHandle(fn_t->as.fn.args_t);

    ffi_status status;
    if (fn_t->as.fn.is_variadic) {
        status = FFI_BAD_ABI;
        // TODO Not handling variadic ffi closure
    } else {
        status = ffi_prep_cif(cif, FFI_DEFAULT_ABI, argc, rtype, atypes->elements);
    }
    assert(status == FFI_OK && "ffi_prep_cif failed");
}

void _ffiClosure(ffi_cif *, void *resp, void **args, void *userdata) {
    NK_LOG_TRC(__func__);

    auto const &cl = *(NkNativeClosure)userdata;

    auto const fn_t = nkval_typeof(cl.fn);

    size_t argc = fn_t->as.fn.args_t->as.tuple.elems.size;

    void *argv = (void *)nk_allocate(nk_default_allocator, fn_t->as.fn.args_t->size);
    defer {
        nk_free(nk_default_allocator, argv);
    };

    nkval_t args_val{argv, fn_t->as.fn.args_t};

    for (size_t i = 0; i < argc; i++) {
        std::memcpy(
            nkval_data(nkval_tuple_at(args_val, i)),
            args[i],
            fn_t->as.fn.args_t->as.tuple.elems.data[i].type->size);
    }

    nkir_invoke(cl.fn, {resp, fn_t->as.fn.ret_t}, args_val);
}

} // namespace

void nk_native_invoke(nkval_t fn, nkval_t ret, nkval_t args) {
    NK_LOG_TRC(__func__);

    size_t const argc = nkval_data(args)
                            ? nkval_typeclassid(args) == NkType_Tuple ? nkval_tuple_size(args) : 1
                            : 0; // TODO Have to handle args not being a tuple

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
            (nkval_data(args) && nkval_typeclassid(args) == NkType_Tuple) ? atypes->elements
                                                                          : &atypes);
    } else {
        status = ffi_prep_cif(
            &cif,
            FFI_DEFAULT_ABI,
            argc,
            rtype,
            (nkval_data(args) && nkval_typeclassid(args) == NkType_Tuple) ? atypes->elements
                                                                          : &atypes);
    }
    assert(status == FFI_OK && "ffi_prep_cif failed");

    void **argv = (void **)nk_allocate(nk_default_allocator, argc * sizeof(void *));
    defer {
        nk_free(nk_default_allocator, argv);
    };

    if (nkval_data(args)) {
        if (nkval_typeclassid(args) == NkType_Tuple) {
            for (size_t i = 0; i < argc; i++) {
                argv[i] = nkval_data(nkval_tuple_at(args, i));
            }
        } else {
            argv[0] = nkval_data(args);
        }
    }

    ffi_call(&cif, FFI_FN(nkval_as(void *, fn)), nkval_data(ret), argv);
}

NkNativeClosure nk_native_make_closure(nkval_t fn) {
    auto cl = (NkNativeClosure)nk_allocate(nk_default_allocator, sizeof(NkNativeClosure_T));
    cl->fn = fn;

    _ffiPrepareCif(&cl->cif, nkval_typeof(fn));

    cl->closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure), &cl->code);

    ffi_status status = ffi_prep_closure_loc(cl->closure, &cl->cif, _ffiClosure, cl, cl->code);
    assert(status == FFI_OK && "ffi_prep_closure_loc failed");

    return cl;
}

void nk_native_free_closure(NkNativeClosure cl) {
    ffi_closure_free(cl->closure);
    nk_free(nk_default_allocator, cl);
}
