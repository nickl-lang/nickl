#ifndef HEADER_GUARD_NKB_TYPES
#define HEADER_GUARD_NKB_TYPES

#include <mutex>

#include <stddef.h>
#include <stdint.h>

#include "nkb/common.h"
#include "ntk/hash_map.hpp"
#include "ntk/string.hpp"

typedef struct {
    NkArena *type_arena;
    NkArena *tmp_arena;
    uint8_t usize;
    NkHashMap<nkstr, nktype_t> fpmap{};
    uint64_t next_id{1};
    std::mutex mtx{};
} NkIrTypeCache;

nktype_t nkir_makeNumericType(NkIrTypeCache *cache, NkIrNumericValueType value_type);
nktype_t nkir_makePointerType(NkIrTypeCache *cache, nktype_t target_type);
nktype_t nkir_makeProcedureType(NkIrTypeCache *cache, NkIrProcInfo proc_info);
nktype_t nkir_makeArrayType(NkIrTypeCache *cache, nktype_t elem_t, size_t count);
nktype_t nkir_makeVoidType(NkIrTypeCache *cache);
nktype_t nkir_makeAggregateType(NkIrTypeCache *cache, nktype_t const *elem_types, size_t const *elem_counts, size_t n);

#endif // HEADER_GUARD_NKB_TYPES
