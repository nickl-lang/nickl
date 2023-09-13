#include "ffi_adapter.hpp"

#include <cassert>
#include <mutex>
#include <unordered_map>

#include <ffi.h>

#include "interp.hpp"
#include "nk/common/allocator.hpp"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nkb/common.h"
#include "nkb/ir.h"

namespace {

NK_LOG_USE_SCOPE(ffi_adapter);

struct Context {
    std::unordered_map<nktype_t, ffi_type *> typemap;
    std::mutex mtx;
    NkArena typearena{};

    ~Context() {
        std::lock_guard lk{mtx};

        nk_arena_free(&typearena);
    }
};

Context ctx;

// TODO Integer promotion works only on little-endian
ffi_type *getNativeHandle(nktype_t type, bool promote = false) {
    EASY_FUNCTION(::profiler::colors::Orange200);
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
        switch (type->kind) {
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
                assert(!"unreachable");
                break;
            }
            break;
        case NkType_Pointer:
        case NkType_Procedure:
            ffi_t = &ffi_type_pointer;
            break;
        case NkType_Aggregate:
            NK_LOG_WRN("TODO NkType_Aggregate translation not implemented");
            break;
        // case NkType_Array: {
        //     auto native_elem_h = getNativeHandle(type->as.arr.elem_type);
        //     ffi_type **elements =
        //         (ffi_type **)nk_arena_alloc(&ctx.typearena, (type->as.arr.elem_count + 1) * sizeof(void *));
        //     std::fill_n(elements, type->as.arr.elem_count, native_elem_h);
        //     elements[type->as.arr.elem_count] = nullptr;
        //     ffi_t = new (nk_arena_alloc(&ctx.typearena, sizeof(ffi_type))) ffi_type{
        //         .size = type->size,
        //         .alignment = type->align,
        //         .type = FFI_TYPE_STRUCT,
        //         .elements = elements,
        //     };
        //     break;
        // }
        // case NkType_Tuple: {
        //     if (!type->as.tuple.elems.size) {
        //         return &ffi_type_void;
        //     }
        //     ffi_type **elements =
        //         (ffi_type **)nk_arena_alloc(&ctx.typearena, (type->as.tuple.elems.size + 1) * sizeof(void *));
        //     for (size_t i = 0; i < type->as.tuple.elems.size; i++) {
        //         elements[i] = getNativeHandle(type->as.tuple.elems.data[i].type, promote);
        //     }
        //     elements[type->as.tuple.elems.size] = nullptr;
        //     ffi_t = new (nk_arena_alloc(&ctx.typearena, sizeof(ffi_type))) ffi_type{
        //         .size = type->size,
        //         .alignment = type->align,
        //         .type = FFI_TYPE_STRUCT,
        //         .elements = elements,
        //     };
        //     break;
        // }
        default:
            assert(!"unreachable");
            break;
        }

        ctx.typemap.emplace(type, ffi_t);
    }

#ifdef ENABLE_LOGGING
    // TODO Boilerplate in logging
    uint8_t type_str[256];
    NkArena log_arena{type_str, 0, sizeof(type_str)};
    NkStringBuilder_T sb{
        nk_arena_alloc_t<char>(&log_arena, sizeof(type_str)),
        0,
        sizeof(type_str),
        nk_arena_getAllocator(&log_arena),
    };
    nkirt_inspect(type, &sb);
    NK_LOG_DBG("ffi(type{name=%s}) -> %p", type_str, (void *)ffi_t);
#endif // ENABLE_LOGGING

    return ffi_t;
}

ffi_type **getNativeHandleArray(NkTypeArray types, bool promote = false) {
    ffi_type **elements = nk_arena_alloc_t<ffi_type *>(&ctx.typearena, types.size + 1);
    for (size_t i = 0; i < types.size; i++) {
        elements[i] = getNativeHandle(types.data[i], promote);
    }
    elements[types.size] = nullptr;
    return elements;
}

void ffiPrepareCif(ffi_cif *cif, size_t nfixedargs, bool is_variadic, ffi_type *rtype, ffi_type **atypes, size_t argc) {
    ffi_status status;
    if (is_variadic) {
        status = ffi_prep_cif_var(cif, FFI_DEFAULT_ABI, nfixedargs, argc, rtype, atypes);
    } else {
        status = ffi_prep_cif(cif, FFI_DEFAULT_ABI, argc, rtype, atypes);
    }
    assert(status == FFI_OK && "ffi_prep_cif failed");
}

struct NkIrNativeClosure_T {
    void *code;
    ffi_cif cif;
    ffi_closure *closure;
    NkBcProc proc;
};

void ffiClosure(ffi_cif *, void *resp, void **args, void *userdata) {
    NK_LOG_TRC("%s", __func__);

    auto const &cl = *(NkIrNativeClosure_T *)userdata;
    nkir_interp_invoke(cl.proc, args, &resp);
}

} // namespace

void nk_native_invoke(
    void *proc,
    size_t nfixedargs,
    bool is_variadic,
    void **argv,
    nktype_t const *argt,
    size_t argc,
    void *ret,
    nktype_t rett) {
    EASY_FUNCTION(::profiler::colors::Orange200);
    NK_LOG_TRC("%s", __func__);

    ffi_cif cif;

    {
        std::lock_guard lk{ctx.mtx};

        // TODO Temporary hack to free memory taken up by the atypes
        auto const frame = nk_arena_grab(&ctx.typearena);
        defer {
            nk_arena_popFrame(&ctx.typearena, frame);
        };

        auto const rtype = getNativeHandle(rett);
        auto const atypes = getNativeHandleArray({argt, argc}, is_variadic);

        ffiPrepareCif(&cif, nfixedargs, is_variadic, rtype, atypes, argc);
    }

    {
        EASY_BLOCK("ffi_call", ::profiler::colors::Orange200);
        ffi_call(&cif, FFI_FN(proc), ret, argv);
    }
}

void *nk_native_makeClosure(
    NkAllocator alloc,
    NkBcProc proc,
    bool is_variadic,
    nktype_t const *argt,
    size_t argc,
    nktype_t rett) {
    EASY_FUNCTION(::profiler::colors::Orange200);
    NK_LOG_TRC("%s", __func__);

    NkIrNativeClosure_T *cl;

    {
        std::lock_guard lk{ctx.mtx};

        cl = nk_alloc_t<NkIrNativeClosure_T>(alloc);
        cl->proc = proc;

        auto const rtype = getNativeHandle(rett);
        auto const atypes = getNativeHandleArray({argt, argc}, is_variadic);

        ffiPrepareCif(&cl->cif, argc, is_variadic, rtype, atypes, argc);
    }

    cl->closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure), &cl->code);

    ffi_status status = ffi_prep_closure_loc(cl->closure, &cl->cif, ffiClosure, cl, cl->code);
    assert(status == FFI_OK && "ffi_prep_closure_loc failed");

    return cl;
}
