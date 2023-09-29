#include "types.hpp"

#include <cstring>
#include <new>

#include "nk/common/allocator.hpp"
#include "nk/common/array.h"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nkb/common.h"

namespace {

NK_LOG_USE_SCOPE(types);

nkar_typedef(char, ByteArray);

template <class F>
nktype_t getTypeByFp(NkIrTypeCache *cache, ByteArray fp, F const &make_type) {
    EASY_FUNCTION(::profiler::colors::Green200);
    NK_LOG_TRC("%s", __func__);

    std::lock_guard lk{cache->mtx};

    auto found = cache->fpmap.find({nkav_init(fp)});
    if (found) {
        return *found;
    } else {
        auto copy = nk_strcpy(nk_arena_getAllocator(cache->type_arena), {nkav_init(fp)});
        return cache->fpmap.insert(copy, make_type());
    }
}

template <class T>
void pushVal(ByteArray &ar, T const v) {
    nkar_reserve(&ar, ar.size + sizeof(v));
    memcpy(ar.data + ar.size, &v, sizeof(v));
    ar.size += sizeof(v);
}

} // namespace

nktype_t nkir_makeNumericType(NkIrTypeCache *cache, NkIrNumericValueType value_type) {
    auto const kind = NkType_Numeric;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(cache->tmp_arena);
    defer {
        nkar_free(&fp);
    };
    pushVal(fp, kind);
    pushVal(fp, value_type);

    return getTypeByFp(cache, fp, [&]() {
        return new (nk_arena_alloc_t<NkIrType>(cache->type_arena)) NkIrType{
            .as{.num{
                .value_type = value_type,
            }},
            .size = (uint8_t)NKIR_NUMERIC_TYPE_SIZE(value_type),
            .align = (uint8_t)NKIR_NUMERIC_TYPE_SIZE(value_type),
            .kind = kind,
            .id = cache->next_id++,
        };
    });
}

nktype_t nkir_makePointerType(NkIrTypeCache *cache, nktype_t target_type) {
    auto const kind = NkType_Pointer;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(cache->tmp_arena);
    defer {
        nkar_free(&fp);
    };
    pushVal(fp, kind);
    pushVal(fp, target_type->id);

    return getTypeByFp(cache, fp, [&]() {
        return new (nk_arena_alloc_t<NkIrType>(cache->type_arena)) NkIrType{
            .as{.ptr{
                .target_type = target_type,
            }},
            .size = cache->usize,
            .align = cache->usize,
            .kind = NkType_Pointer,
            .id = cache->next_id++,
        };
    });
}

nktype_t nkir_makeProcedureType(NkIrTypeCache *cache, NkIrProcInfo proc_info) {
    auto const kind = NkType_Procedure;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(cache->tmp_arena);
    defer {
        nkar_free(&fp);
    };
    pushVal(fp, kind);
    for (size_t i = 0; i < proc_info.args_t.size; i++) {
        pushVal(fp, proc_info.args_t.data[i]->id);
    }
    for (size_t i = 0; i < proc_info.ret_t.size; i++) {
        pushVal(fp, proc_info.ret_t.data[i]->id);
    }
    pushVal(fp, proc_info.call_conv);
    pushVal(fp, proc_info.flags);

    return getTypeByFp(cache, fp, [&]() {
        return new (nk_arena_alloc_t<NkIrType>(cache->type_arena)) NkIrType{
            .as{.proc{
                .info = proc_info,
            }},
            .size = cache->usize,
            .align = cache->usize,
            .kind = NkType_Procedure,
            .id = cache->next_id++,
        };
    });
}

nktype_t nkir_makeArrayType(NkIrTypeCache *cache, nktype_t elem_t, size_t count) {
    auto const kind = NkType_Aggregate;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(cache->tmp_arena);
    defer {
        nkar_free(&fp);
    };
    pushVal(fp, kind);
    pushVal(fp, size_t{1});
    pushVal(fp, elem_t->id);
    pushVal(fp, count);

    return getTypeByFp(cache, fp, [&]() {
        return new (nk_arena_alloc_t<NkIrType>(cache->type_arena)) NkIrType{
            .as{.aggr{
                .elems{
                    new (nk_arena_alloc_t<NkIrAggregateElemInfo>(cache->type_arena)) NkIrAggregateElemInfo{
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
            .id = cache->next_id++,
        };
    });
}

nktype_t nkir_makeVoidType(NkIrTypeCache *cache) {
    auto const kind = NkType_Aggregate;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(cache->tmp_arena);
    defer {
        nkar_free(&fp);
    };
    pushVal(fp, kind);
    pushVal(fp, size_t{});

    return getTypeByFp(cache, fp, [&]() {
        return new (nk_arena_alloc_t<NkIrType>(cache->type_arena)) NkIrType{
            .as{.aggr{.elems{nullptr, 0}}},
            .size = 0,
            .align = 1,
            .kind = NkType_Aggregate,
            .id = cache->next_id++,
        };
    });
}

nktype_t nkir_makeAggregateType(NkIrTypeCache *cache, nktype_t const *elem_types, size_t const *elem_counts, size_t n) {
    auto const kind = NkType_Aggregate;

    ByteArray fp{};
    fp.alloc = nk_arena_getAllocator(cache->tmp_arena);
    defer {
        nkar_free(&fp);
    };
    pushVal(fp, kind);
    pushVal(fp, n);
    for (size_t i = 0; i < n; i++) {
        pushVal(fp, elem_types[i]->id);
        pushVal(fp, elem_counts[i]);
    }

    return getTypeByFp(cache, fp, [&]() {
        auto const layout =
            nkir_calcAggregateLayout(nk_arena_getAllocator(cache->type_arena), elem_types, elem_counts, n, 1);

        return new (nk_arena_alloc_t<NkIrType>(cache->type_arena)) NkIrType{
            .as{.aggr{
                .elems = layout.info_ar,
            }},
            .size = layout.size,
            .align = (uint8_t)layout.align,
            .kind = NkType_Aggregate,
            .id = cache->next_id++,
        };
    });
}
