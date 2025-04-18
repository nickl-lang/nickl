#include "native_fn_adapter.h"

#include <cstring>
#include <mutex>
#include <unordered_map>

#if defined(__APPLE__)
#include <ffi/ffi.h>
#else
#include <ffi.h>
#endif

#include "ir_impl.hpp"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/value.h"
#include "ntk/allocator.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

struct NkIrNativeClosure_T {
    void *code; // Assumed to be first field
    ffi_cif cif;
    ffi_closure *closure;
    NkIrFunct fn;
};

namespace {

NK_LOG_USE_SCOPE(native_fn_adapter);

struct Context {
    std::unordered_map<nktype_t, ffi_type *> typemap;
    std::mutex mtx;
    NkArena typearena{};

    void deinit() {
        std::lock_guard lk{mtx};

        typemap.clear();
        nk_arena_free(&typearena);
    }

    ~Context() {
        deinit();
    }
};

static Context ctx;

// TODO Integer promotion works only on little-endian
ffi_type *_getNativeHandle(nktype_t type, bool promote = false) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    if (!type) {
        NK_LOG_DBG("ffi(null) -> void");
        return &ffi_type_void;
    }

    ffi_type *ffi_t = nullptr;

    auto it = ctx.typemap.find(type);
    if (it != ctx.typemap.end()) {
        NK_LOG_DBG("Found existing ffi type=%p", (void *)it->second);
        ffi_t = it->second;
    } else {
        switch (type->tclass) {
            case NkType_Array: {
                auto native_elem_h = _getNativeHandle(type->as.arr.elem_type);
                ffi_type **elements = (ffi_type **)nk_arena_allocAligned(
                    &ctx.typearena, (type->as.arr.elem_count + 1) * sizeof(void *), alignof(void *));
                std::fill_n(elements, type->as.arr.elem_count, native_elem_h);
                elements[type->as.arr.elem_count] = nullptr;
                ffi_t = new (nk_arena_allocAligned(&ctx.typearena, sizeof(ffi_type), alignof(ffi_type))) ffi_type{
                    .size = type->size,
                    .alignment = type->align,
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
                        ffi_t = promote ? &ffi_type_sint32 : &ffi_type_sint8;
                        break;
                    case Int16:
                        ffi_t = promote ? &ffi_type_sint32 : &ffi_type_sint16;
                        break;
                    case Int32:
                        ffi_t = &ffi_type_sint32;
                        break;
                    case Int64:
                        ffi_t = &ffi_type_sint64;
                        break;
                    case Uint8:
                        ffi_t = promote ? &ffi_type_uint32 : &ffi_type_uint8;
                        break;
                    case Uint16:
                        ffi_t = promote ? &ffi_type_uint32 : &ffi_type_uint16;
                        break;
                    case Uint32:
                        ffi_t = &ffi_type_uint32;
                        break;
                    case Uint64:
                        ffi_t = &ffi_type_uint64;
                        break;
                    case Float32:
                        ffi_t = promote ? &ffi_type_double : &ffi_type_float;
                        break;
                    case Float64:
                        ffi_t = &ffi_type_double;
                        break;
                    default:
                        nk_assert(!"unreachable");
                        break;
                }
                break;
            case NkType_Ptr:
                ffi_t = &ffi_type_pointer;
                break;
            case NkType_Tuple: {
                if (!type->as.tuple.elems.size) {
                    return &ffi_type_void;
                }
                ffi_type **elements = (ffi_type **)nk_arena_allocAligned(
                    &ctx.typearena, (type->as.tuple.elems.size + 1) * sizeof(void *), alignof(void *));
                for (usize i = 0; i < type->as.tuple.elems.size; i++) {
                    elements[i] = _getNativeHandle(type->as.tuple.elems.data[i].type, promote);
                }
                elements[type->as.tuple.elems.size] = nullptr;
                ffi_t = new (nk_arena_allocAligned(&ctx.typearena, sizeof(ffi_type), alignof(ffi_type))) ffi_type{
                    .size = type->size,
                    .alignment = type->align,
                    .type = FFI_TYPE_STRUCT,
                    .elements = elements,
                };
                break;
            }
            default:
                nk_assert(!"unreachable");
                break;
        }

        ctx.typemap.emplace(type, ffi_t);
    }

#ifdef ENABLE_LOGGING
    {
        NkStringBuilder sb{};
        defer {
            nksb_free(&sb);
        };
        nkt_inspect(type, &sb);
        NK_LOG_DBG("ffi(type{name=" NKS_FMT "}) -> %p", NKS_ARG(sb), (void *)ffi_t);
    }
#endif // ENABLE_LOGGING

    return ffi_t;
}

void _ffiPrepareCif(ffi_cif *cif, usize actual_argc, bool is_variadic, ffi_type *rtype, ffi_type *atypes, usize argc) {
    ffi_status status;
    if (is_variadic) {
        status = ffi_prep_cif_var(cif, FFI_DEFAULT_ABI, actual_argc, argc, rtype, atypes->elements);
    } else {
        status = ffi_prep_cif(cif, FFI_DEFAULT_ABI, argc, rtype, atypes->elements);
    }
    nk_assert(status == FFI_OK && "ffi_prep_cif failed");
}

void _ffiClosure(ffi_cif *, void *resp, void **args, void *userdata) {
    NK_LOG_TRC("%s", __func__);

    auto const &cl = *(NkIrNativeClosure)userdata;

    auto const fn_t = cl.fn->fn_t;

    usize argc = fn_t->as.fn.args_t->as.tuple.elems.size;

    void *argv = (void *)nk_alloc(nk_default_allocator, fn_t->as.fn.args_t->size);
    defer {
        nk_free(nk_default_allocator, argv, fn_t->as.fn.args_t->size);
    };

    nkval_t args_val{argv, fn_t->as.fn.args_t};

    for (usize i = 0; i < argc; i++) {
        std::memcpy(
            nkval_data(nkval_tuple_at(args_val, i)), args[i], fn_t->as.fn.args_t->as.tuple.elems.data[i].type->size);
    }

    nkir_invoke({(void *)&cl.fn, fn_t}, {resp, fn_t->as.fn.ret_t}, args_val);
}

} // namespace

void nk_native_invoke(nkval_t fn, nkval_t ret, nkval_t args) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    usize const argc = nkval_data(args) ? nkval_tuple_size(args) : 0;

    auto const fn_t = nkval_typeof(fn);
    bool const is_variadic = fn_t->as.fn.is_variadic;

    ffi_cif cif;

    {
        std::lock_guard lk{ctx.mtx};

        auto const rtype = _getNativeHandle(nkval_typeof(ret));
        auto const atypes = _getNativeHandle(nkval_typeof(args), is_variadic);

        _ffiPrepareCif(&cif, fn_t->as.fn.args_t->as.tuple.elems.size, is_variadic, rtype, atypes, argc);
    }

    void **argv = (void **)nk_alloc(nk_default_allocator, argc * sizeof(void *));
    defer {
        nk_free(nk_default_allocator, argv, argc * sizeof(void *));
    };

    if (nkval_data(args)) {
        for (usize i = 0; i < argc; i++) {
            argv[i] = nkval_data(nkval_tuple_at(args, i));
        }
    }

    {
        NK_PROF_SCOPE(nk_cs2s("ffi_call"));
        ffi_call(&cif, FFI_FN(nkval_as(void *, fn)), nkval_data(ret), argv);
    }
}

NkIrNativeClosure nk_native_make_closure(NkIrFunct fn) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    NkIrNativeClosure cl;

    {
        std::lock_guard lk{ctx.mtx};

        cl = (NkIrNativeClosure)nk_alloc(nk_default_allocator, sizeof(NkIrNativeClosure_T));
        cl->fn = fn;

        usize const argc = fn->fn_t->as.fn.args_t->as.tuple.elems.size;

        bool const is_variadic = fn->fn_t->as.fn.is_variadic;

        auto const rtype = _getNativeHandle(fn->fn_t->as.fn.ret_t);
        auto const atypes = _getNativeHandle(fn->fn_t->as.fn.args_t, is_variadic);

        _ffiPrepareCif(&cl->cif, argc, is_variadic, rtype, atypes, argc);
    }

    cl->closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure), &cl->code);

    ffi_status status = ffi_prep_closure_loc(cl->closure, &cl->cif, _ffiClosure, cl, cl->code);
    nk_assert(status == FFI_OK && "ffi_prep_closure_loc failed");

    return cl;
}

void nk_native_free_closure(NkIrNativeClosure cl) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    ffi_closure_free(cl->closure);
    nk_free(nk_default_allocator, cl, sizeof(*cl));
}

void nk_native_adapterDeinit() {
    ctx.deinit();
}
