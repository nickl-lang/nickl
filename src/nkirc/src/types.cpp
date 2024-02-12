#include "types.h"

#include <cstring>

#include "irc_impl.hpp"
#include "nkb/common.h"
#include "ntk/allocator.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/profiler.h"

namespace {

NK_LOG_USE_SCOPE(types);

typedef NkDynArray(char) ByteArray;

template <class F>
nktype_t getTypeByFp(NkIrCompiler c, ByteArray fp, F const &make_type) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    std::lock_guard lk{c->mtx};

    auto found = c->fpmap.find({NK_SLICE_INIT(fp)});
    if (found) {
        return *found;
    } else {
        auto copy = nks_copy(nk_arena_getAllocator(&c->file_arena), {NK_SLICE_INIT(fp)});
        return c->fpmap.insert(copy, make_type());
    }
}

template <class T>
void pushVal(ByteArray &ar, T const v) {
    nkda_reserve(&ar, ar.size + sizeof(v));
    memcpy(nk_slice_end(&ar), &v, sizeof(v));
    ar.size += sizeof(v);
}

} // namespace

nktype_t nkir_makeNumericType(NkIrCompiler c, NkIrNumericValueType value_type) {
    auto const kind = NkType_Numeric;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(c->tmp_arena);
    defer {
        nkda_free(&fp);
    };
    pushVal(fp, kind);
    pushVal(fp, value_type);

    return getTypeByFp(c, fp, [&]() {
        return new (nk_arena_allocT<NkIrType>(&c->file_arena)) NkIrType{
            .as{.num{
                .value_type = value_type,
            }},
            .size = (u8)NKIR_NUMERIC_TYPE_SIZE(value_type),
            .align = (u8)NKIR_NUMERIC_TYPE_SIZE(value_type),
            .kind = kind,
            .id = c->next_id++,
        };
    });
}

nktype_t nkir_makePointerType(NkIrCompiler c, nktype_t target_type) {
    auto const kind = NkType_Pointer;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(c->tmp_arena);
    defer {
        nkda_free(&fp);
    };
    pushVal(fp, kind);
    pushVal(fp, target_type->id);

    return getTypeByFp(c, fp, [&]() {
        return new (nk_arena_allocT<NkIrType>(&c->file_arena)) NkIrType{
            .as{.ptr{
                .target_type = target_type,
            }},
            .size = c->conf.ptr_size,
            .align = c->conf.ptr_size,
            .kind = NkType_Pointer,
            .id = c->next_id++,
        };
    });
}

nktype_t nkir_makeProcedureType(NkIrCompiler c, NkIrProcInfo proc_info) {
    auto const kind = NkType_Procedure;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(c->tmp_arena);
    defer {
        nkda_free(&fp);
    };
    pushVal(fp, kind);
    for (usize i = 0; i < proc_info.args_t.size; i++) {
        pushVal(fp, proc_info.args_t.data[i]->id);
    }
    pushVal(fp, proc_info.ret_t->id);
    pushVal(fp, proc_info.call_conv);
    pushVal(fp, proc_info.flags);

    return getTypeByFp(c, fp, [&]() {
        return new (nk_arena_allocT<NkIrType>(&c->file_arena)) NkIrType{
            .as{.proc{
                .info = proc_info,
            }},
            .size = c->conf.ptr_size,
            .align = c->conf.ptr_size,
            .kind = NkType_Procedure,
            .id = c->next_id++,
        };
    });
}

nktype_t nkir_makeArrayType(NkIrCompiler c, nktype_t elem_t, usize count) {
    auto const kind = NkType_Aggregate;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(c->tmp_arena);
    defer {
        nkda_free(&fp);
    };
    pushVal(fp, kind);
    pushVal(fp, usize{1});
    pushVal(fp, elem_t->id);
    pushVal(fp, count);

    return getTypeByFp(c, fp, [&]() {
        return new (nk_arena_allocT<NkIrType>(&c->file_arena)) NkIrType{
            .as{.aggr{
                .elems{
                    new (nk_arena_allocT<NkIrAggregateElemInfo>(&c->file_arena)) NkIrAggregateElemInfo{
                        .type = elem_t,
                        .count = count,
                        .offset = 0,
                    },
                    1,
                },
            }},
            .size = elem_t->size * count,
            .align = elem_t->align,
            .kind = NkType_Aggregate,
            .id = c->next_id++,
        };
    });
}

nktype_t nkir_makeVoidType(NkIrCompiler c) {
    auto const kind = NkType_Aggregate;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(c->tmp_arena);
    defer {
        nkda_free(&fp);
    };
    pushVal(fp, kind);
    pushVal(fp, usize{});

    return getTypeByFp(c, fp, [&]() {
        return new (nk_arena_allocT<NkIrType>(&c->file_arena)) NkIrType{
            .as{.aggr{.elems{nullptr, 0}}},
            .size = 0,
            .align = 1,
            .kind = NkType_Aggregate,
            .id = c->next_id++,
        };
    });
}

nktype_t nkir_makeAggregateType(NkIrCompiler c, nktype_t const *elem_types, usize const *elem_counts, usize n) {
    auto const kind = NkType_Aggregate;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(c->tmp_arena);
    defer {
        nkda_free(&fp);
    };
    pushVal(fp, kind);
    pushVal(fp, n);
    for (usize i = 0; i < n; i++) {
        pushVal(fp, elem_types[i]->id);
        pushVal(fp, elem_counts[i]);
    }

    return getTypeByFp(c, fp, [&]() {
        auto const layout =
            nkir_calcAggregateLayout(nk_arena_getAllocator(&c->file_arena), elem_types, elem_counts, n, 1, 1);

        return new (nk_arena_allocT<NkIrType>(&c->file_arena)) NkIrType{
            .as{.aggr{
                .elems = layout.info_ar,
            }},
            .size = layout.size,
            .align = (u8)layout.align,
            .kind = NkType_Aggregate,
            .id = c->next_id++,
        };
    });
}
