#include "ffi_adapter.hpp"

#include <cassert>
#include <mutex>
#include <unordered_map>

#include <ffi.h>

#include "nk/common/allocator.h"
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
    std::recursive_mutex mtx;
    NkArena typearena{};

    ~Context() {
        nk_arena_free(&typearena);
    }
};

Context ctx;

ffi_type *getNativeHandle(NkTypeArray types, bool promote = false) {
}

// TODO Integer promotion works only on little-endian
ffi_type *getNativeHandle(nktype_t type, bool promote = false) {
    EASY_FUNCTION(::profiler::colors::Orange200);
    NK_LOG_TRC("%s", __func__);

    if (!type) {
        NK_LOG_DBG("ffi(null) -> void");
        return &ffi_type_void;
    }

    std::lock_guard lk{ctx.mtx};

    ffi_type *ffi_t = nullptr;

    auto it = ctx.typemap.find(type);
    if (it != ctx.typemap.end()) {
        NK_LOG_DBG("Found existing ffi type=%p", (void *)it->second);
        ffi_t = it->second;
    } else {
        switch (type->kind) {
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
        case NkType_Basic:
            switch (type->as.basic.value_type) {
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

    NK_LOG_DBG(
        "ffi(type{name=%s}) -> %p",
        (char const *)[&]() {
            auto sb = nksb_create();
            nkirt_inspect(type, sb);
            return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                nksb_free(sb);
            });
        }(),
        (void *)ffi_t);

    return ffi_t;
}

void ffiPrepareCif(ffi_cif *cif, size_t actual_argc, bool is_variadic, ffi_type *rtype, ffi_type *atypes, size_t argc) {
    ffi_status status;
    if (is_variadic) {
        status = ffi_prep_cif_var(cif, FFI_DEFAULT_ABI, actual_argc, argc, rtype, atypes->elements);
    } else {
        status = ffi_prep_cif(cif, FFI_DEFAULT_ABI, argc, rtype, atypes->elements);
    }
    assert(status == FFI_OK && "ffi_prep_cif failed");
}

} // namespace

void nk_native_invoke(void *proc, NkIrProcInfo const *proc_info, void **argv, size_t argc, void *ret) {
    EASY_FUNCTION(::profiler::colors::Orange200);
    NK_LOG_TRC("%s", __func__);

    bool const is_variadic = proc_info->flags & NkProcVariadic;

    auto const rtype = getNativeHandle(proc_info->ret_t.data[0]);
    auto const atypes = getNativeHandle(proc_info->args_t, is_variadic);

    ffi_cif cif;
    ffiPrepareCif(&cif, proc_info->args_t.size, is_variadic, rtype, atypes, argc);

    {
        EASY_BLOCK("ffi_call", ::profiler::colors::Orange200);
        ffi_call(&cif, FFI_FN(proc), ret, argv);
    }
}
