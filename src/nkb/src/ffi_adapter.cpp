#include "ffi_adapter.h"

#if defined(__APPLE__)
#include <ffi/ffi.h>
#else
#include <ffi.h>
#endif

#include "interp.h"
#include "nkb/common.h"
#include "ntk/allocator.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string_builder.h"
#include "ntk/thread.h"

namespace {

NK_LOG_USE_SCOPE(ffi_adapter);

ffi_type *getNativeHandle(NkFfiContext *ctx, nktype_t type) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    if (!type) {
        NK_LOG_DBG("ffi(null) -> void");
        return &ffi_type_void;
    }

    ffi_type *ffi_t = nullptr;

    auto found = TypeTree_findItem(&ctx->types, type->id);
    if (found) {
        NK_LOG_DBG("Found existing ffi type=%p", found->val);
        ffi_t = (ffi_type *)found->val;
    } else {
        switch (type->kind) {
            case NkIrType_Numeric:
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
                        nk_assert(!"unreachable");
                        break;
                }
                break;
            case NkIrType_Pointer:
            case NkIrType_Procedure:
                ffi_t = &ffi_type_pointer;
                break;
            case NkIrType_Aggregate: {
                if (!type->size) {
                    return &ffi_type_void;
                }
                usize elem_count = 0;
                for (usize i = 0; i < type->as.aggr.elems.size; i++) {
                    elem_count += type->as.aggr.elems.data[i].count;
                }
                ffi_type **elems = (ffi_type **)nk_allocT<void *>(ctx->alloc, elem_count + 1);
                usize cur_elem = 0;
                for (usize i = 0; i < type->as.aggr.elems.size; i++) {
                    for (usize j = 0; j < type->as.aggr.elems.data[i].count; j++) {
                        elems[cur_elem++] = getNativeHandle(ctx, type->as.aggr.elems.data[i].type);
                    }
                }
                elems[elem_count] = nullptr;
                ffi_t = new (nk_allocT<ffi_type>(ctx->alloc)) ffi_type{
                    .size = type->size,
                    .alignment = type->align,
                    .type = FFI_TYPE_STRUCT,
                    .elements = elems,
                };
                break;
            }
            default:
                nk_assert(!"unreachable");
                break;
        }

        TypeTree_insertItem(&ctx->types, {type->id, ffi_t});
    }

#ifdef ENABLE_LOGGING
    NKSB_FIXED_BUFFER(sb, 256);
    nkirt_inspect(type, nksb_getStream(&sb));
    NK_LOG_DBG("ffi(type{name=" NKS_FMT " id=%" PRIu32 "}) -> %p", NKS_ARG(sb), type->id, (void *)ffi_t);
#endif // ENABLE_LOGGING

    return ffi_t;
}

ffi_type **getNativeHandleArray(NkFfiContext *ctx, NkArena *stack, NkTypeArray types) {
    ffi_type **elements = nk_arena_allocT<ffi_type *>(stack, types.size + 1);
    for (usize i = 0; i < types.size; i++) {
        elements[i] = getNativeHandle(ctx, types.data[i]);
    }
    elements[types.size] = nullptr;
    return (ffi_type **)elements;
}

void ffiPrepareCif(ffi_cif *cif, usize nfixedargs, bool is_variadic, ffi_type *rtype, ffi_type **atypes, usize argc) {
    ffi_status status;
    if (is_variadic) {
        status = ffi_prep_cif_var(cif, FFI_DEFAULT_ABI, nfixedargs, argc, rtype, atypes);
    } else {
        status = ffi_prep_cif(cif, FFI_DEFAULT_ABI, argc, rtype, atypes);
    }
    nk_assert(status == FFI_OK && "ffi_prep_cif failed");
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
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    ffi_cif cif;

    NK_MUTEX_GUARD_SCOPE(ctx->mtx) {
        auto const rtype = getNativeHandle(ctx, call_data->rett);
        auto const atypes = getNativeHandleArray(ctx, stack, {call_data->argt, call_data->argc});

        ffiPrepareCif(&cif, call_data->nfixedargs, call_data->is_variadic, rtype, atypes, call_data->argc);
    }

    {
        NK_PROF_SCOPE(nk_cs2s("ffi_call"));
        ffi_call(&cif, FFI_FN(call_data->proc.native), call_data->retv, call_data->argv);
    }
}

void *nk_native_makeClosure(NkFfiContext *ctx, NkArena *stack, NkAllocator alloc, NkNativeCallData const *call_data) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    NkIrNativeClosure_T *cl;

    NK_MUTEX_GUARD_SCOPE(ctx->mtx) {
        cl = nk_allocT<NkIrNativeClosure_T>(alloc);
        cl->proc = call_data->proc.bytecode;

        auto const rtype = getNativeHandle(ctx, call_data->rett);
        auto const atypes = getNativeHandleArray(ctx, stack, {call_data->argt, call_data->argc});

        ffiPrepareCif(&cl->cif, call_data->nfixedargs, call_data->is_variadic, rtype, atypes, call_data->argc);
    }

    cl->closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure), &cl->code);

    ffi_status status = ffi_prep_closure_loc(cl->closure, &cl->cif, ffiClosure, cl, cl->code);
    nk_assert(status == FFI_OK && "ffi_prep_closure_loc failed");

    return cl;
}
