#include "native_fn_adapter.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <unordered_map>

#include <ffi.h>

#include "ir_impl.hpp"
#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/value.h"

struct NkIrNativeClosure_T {
    void *code; // Assumed to be first field
    ffi_cif cif;
    ffi_closure *closure;
    NkIrFunct fn;
};

namespace {

NK_LOG_USE_SCOPE(native_fn_adapter);

std::unordered_map<nk_typeid_t, ffi_type *> s_typemap;
NkAllocator *s_typearena;
std::recursive_mutex s_mtx;

auto s_deinit_typearena = makeDeferrer([]() {
    if (s_typearena) {
        nk_free_arena(s_typearena);
    }
});

ffi_type *_getNativeHandle(nktype_t type) {
    EASY_FUNCTION(::profiler::colors::Orange200);
    NK_LOG_TRC(__func__);

    if (!type) {
        NK_LOG_DBG("ffi(null) -> void");
        return &ffi_type_void;
    }

    std::lock_guard lk{s_mtx};

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
            ffi_type **elements = (ffi_type **)nk_allocate(
                s_typearena, (type->as.arr.elem_count + 1) * sizeof(void *));
            std::fill_n(elements, type->as.arr.elem_count, native_elem_h);
            elements[type->as.arr.elem_count] = nullptr;
            ffi_t = new (nk_allocate(s_typearena, sizeof(ffi_type))) ffi_type{
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
            ffi_type **elements = (ffi_type **)nk_allocate(
                s_typearena, (type->as.tuple.elems.size + 1) * sizeof(void *));
            for (size_t i = 0; i < type->as.tuple.elems.size; i++) {
                elements[i] = _getNativeHandle(type->as.tuple.elems.data[i].type);
            }
            elements[type->as.tuple.elems.size] = nullptr;
            ffi_t = new (nk_allocate(s_typearena, sizeof(ffi_type))) ffi_type{
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

    NK_LOG_DBG(
        "ffi(type{id=%llu name=%s}) -> %p",
        type->id,
        (char const *)[&]() {
            auto sb = nksb_create();
            nkt_inspect(type, sb);
            return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                nksb_free(sb);
            });
        }(),
        ffi_t);

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

    auto const &cl = *(NkIrNativeClosure)userdata;

    auto const fn_t = cl.fn->fn_t;

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

    nkir_invoke({(void *)&cl.fn, fn_t}, {resp, fn_t->as.fn.ret_t}, args_val);
}

} // namespace

void nk_native_invoke(nkval_t fn, nkval_t ret, nkval_t args) {
    EASY_FUNCTION(::profiler::colors::Orange200);
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

    void **argv = (void **)nk_allocate(nk_default_allocator, argc * sizeof(void *));
    defer {
        nk_free(nk_default_allocator, argv);
    };

    if (nkval_data(args)) {
        for (size_t i = 0; i < argc; i++) {
            argv[i] = nkval_data(nkval_tuple_at(args, i));
        }
    }

    {
        EASY_BLOCK("ffi_call", ::profiler::colors::Orange200);
        ffi_call(&cif, FFI_FN(nkval_as(void *, fn)), nkval_data(ret), argv);
    }
}

NkIrNativeClosure nk_native_make_closure(NkIrFunct fn) {
    EASY_FUNCTION(::profiler::colors::Orange200);
    NK_LOG_TRC(__func__);

    auto cl = (NkIrNativeClosure)nk_allocate(nk_default_allocator, sizeof(NkIrNativeClosure_T));
    cl->fn = fn;

    _ffiPrepareCif(&cl->cif, fn->fn_t);

    cl->closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure), &cl->code);

    ffi_status status = ffi_prep_closure_loc(cl->closure, &cl->cif, _ffiClosure, cl, cl->code);
    assert(status == FFI_OK && "ffi_prep_closure_loc failed");

    return cl;
}

void nk_native_free_closure(NkIrNativeClosure cl) {
    EASY_FUNCTION(::profiler::colors::Orange200);
    NK_LOG_TRC(__func__);

    ffi_closure_free(cl->closure);
    nk_free(nk_default_allocator, cl);
}
