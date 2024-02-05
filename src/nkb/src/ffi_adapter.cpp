#include "ffi_adapter.hpp"

#include <cassert>
#include <new>

#include <ffi.h>

#include "interp.hpp"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "ntk/allocator.h"
#include "ntk/logger.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(ffi_adapter);

ffi_type *getNativeHandle(NkFfiContext *ctx, nktype_t type) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    if (!type) {
        NK_LOG_DBG("ffi(null) -> void");
        ProfEndBlock();
        return &ffi_type_void;
    }

    ffi_type *ffi_t = nullptr;

    auto found = ctx->typemap.find(type->id);
    if (found) {
        NK_LOG_DBG("Found existing ffi type=%p", *found);
        ffi_t = (ffi_type *)*found;
    } else {
        switch (type->kind) {
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
        case NkType_Pointer:
        case NkType_Procedure:
            ffi_t = &ffi_type_pointer;
            break;
        case NkType_Aggregate: {
            if (!type->size) {
                ProfEndBlock();
                return &ffi_type_void;
            }
            size_t elem_count = 0;
            for (size_t i = 0; i < type->as.aggr.elems.size; i++) {
                elem_count += type->as.aggr.elems.data[i].count;
            }
            ffi_type **elems = (ffi_type **)nk_alloc_t<void *>(ctx->alloc, elem_count + 1);
            size_t cur_elem = 0;
            for (size_t i = 0; i < type->as.aggr.elems.size; i++) {
                for (size_t j = 0; j < type->as.aggr.elems.data[i].count; j++) {
                    elems[cur_elem++] = getNativeHandle(ctx, type->as.aggr.elems.data[i].type);
                }
            }
            elems[elem_count] = nullptr;
            ffi_t = new (nk_alloc_t<ffi_type>(ctx->alloc)) ffi_type{
                .size = type->size,
                .alignment = type->align,
                .type = FFI_TYPE_STRUCT,
                .elements = elems,
            };
            break;
        }
        default:
            assert(!"unreachable");
            break;
        }

        ctx->typemap.insert(type->id, ffi_t);
    }

#ifdef ENABLE_LOGGING
    nksb_fixed_buffer(sb, 256);
    nkirt_inspect(type, nksb_getStream(&sb));
    NK_LOG_DBG("ffi(type{name=" nks_Fmt " id=%" PRIu64 "}) -> %p", nks_Arg(sb), type->id, (void *)ffi_t);
#endif // ENABLE_LOGGING

    ProfEndBlock();
    return ffi_t;
}

ffi_type **getNativeHandleArray(NkFfiContext *ctx, NkArena *stack, NkTypeArray types) {
    ffi_type **elements = nk_arena_alloc_t<ffi_type *>(stack, types.size + 1);
    for (size_t i = 0; i < types.size; i++) {
        elements[i] = getNativeHandle(ctx, types.data[i]);
    }
    elements[types.size] = nullptr;
    return (ffi_type **)elements;
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

void nk_native_invoke(NkFfiContext *ctx, NkArena *stack, NkNativeCallData const *call_data) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    ffi_cif cif;

    {
        std::lock_guard lk{ctx->mtx};

        auto const rtype = getNativeHandle(ctx, call_data->rett);
        auto const atypes = getNativeHandleArray(ctx, stack, {call_data->argt, call_data->argc});

        ffiPrepareCif(&cif, call_data->nfixedargs, call_data->is_variadic, rtype, atypes, call_data->argc);
    }

    {
        ProfBeginBlock("ffi_call", sizeof("ffi_call") - 1);
        ffi_call(&cif, FFI_FN(call_data->proc.native), call_data->retv, call_data->argv);
        ProfEndBlock();
    }

    ProfEndBlock();
}

void *nk_native_makeClosure(NkFfiContext *ctx, NkArena *stack, NkAllocator alloc, NkNativeCallData const *call_data) {
    ProfBeginFunc();
    NK_LOG_TRC("%s", __func__);

    NkIrNativeClosure_T *cl;

    {
        std::lock_guard lk{ctx->mtx};

        cl = nk_alloc_t<NkIrNativeClosure_T>(alloc);
        cl->proc = call_data->proc.bytecode;

        auto const rtype = getNativeHandle(ctx, call_data->rett);
        auto const atypes = getNativeHandleArray(ctx, stack, {call_data->argt, call_data->argc});

        ffiPrepareCif(&cl->cif, call_data->nfixedargs, call_data->is_variadic, rtype, atypes, call_data->argc);
    }

    cl->closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure), &cl->code);

    ffi_status status = ffi_prep_closure_loc(cl->closure, &cl->cif, ffiClosure, cl, cl->code);
    assert(status == FFI_OK && "ffi_prep_closure_loc failed");

    ProfEndBlock();
    return cl;
}
