#include "nkl/lang/types.h"

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>

#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/utils.h"
#include "nk/vm/common.h"
#include "nk/vm/value.h"

// TODO @Optimization A bit excessive approach with type fingerprints
using ByteArray = std::vector<uint8_t>;

namespace {

NK_LOG_USE_SCOPE(types);

static std::deque<NkType> s_types;
static std::map<ByteArray, nktype_t> s_typemap;
static std::mutex s_mutex;

template <class F>
nktype_t getTypeByFingerprint(ByteArray fp, nk_typeclassid_t tclass, F const &create_vm_type) {
    EASY_FUNCTION(); // TODO Choose color
    NK_LOG_TRC(__func__);

    (void)tclass; // TODO Unused while we don't extend the actual vm type struct

    std::lock_guard lk{s_mutex};

    auto it = s_typemap.find(fp);
    if (it == s_typemap.end()) {
        bool inserted = false;
        auto type = &s_types.emplace_back(create_vm_type());
        std::tie(it, inserted) = s_typemap.emplace(std::move(fp), type);
        assert(inserted && "type duplication");
    }
    return it->second;
}

template <class T>
void pushVal(ByteArray &ar, T v) {
    ar.insert(ar.end(), (uint8_t *)&v, (uint8_t *)&v + sizeof(v));
}

} // namespace

void nkl_types_clean() {
    std::lock_guard lk{s_mutex};

    s_types.clear();
    s_typemap.clear();
}

nktype_t nkl_get_array(nktype_t elem_type, size_t elem_count) {
    auto const tclass = NkType_Array;

    // TODO Pushing ptrs instead of typeids

    ByteArray fp{};
    pushVal(fp, tclass);
    pushVal(fp, elem_type);
    pushVal(fp, elem_count);

    return getTypeByFingerprint(std::move(fp), tclass, [=]() {
        return nkt_get_array(elem_type, elem_count);
    });
}

nktype_t nkl_get_fn(NktFnInfo info) {
    auto const tclass = NkType_Fn;

    ByteArray fp{};
    pushVal(fp, tclass);
    pushVal(fp, info.ret_t);
    pushVal(fp, info.args_t);
    pushVal(fp, info.call_conv);
    pushVal(fp, info.is_variadic);

    return getTypeByFingerprint(std::move(fp), tclass, [=]() {
        return nkt_get_fn(NktFnInfo{
            .ret_t = info.ret_t,
            .args_t = info.args_t,
            .call_conv = info.call_conv,
            .is_variadic = info.is_variadic,
        });
    });
}

nktype_t nkl_get_numeric(NkNumericValueType value_type) {
    auto const tclass = NkType_Numeric;

    ByteArray fp{};
    pushVal(fp, tclass);
    pushVal(fp, value_type);

    return getTypeByFingerprint(std::move(fp), tclass, [=]() {
        return nkt_get_numeric(value_type);
    });
}

nktype_t nkl_get_ptr(nktype_t target_type) {
    auto const tclass = NkType_Ptr;

    ByteArray fp{};
    pushVal(fp, tclass);
    pushVal(fp, target_type);

    return getTypeByFingerprint(std::move(fp), tclass, [=]() {
        return nkt_get_ptr(target_type);
    });
}

nktype_t nkl_get_tuple(NkAllocator alloc, nktype_t const *types, size_t count, size_t stride) {
    auto const tclass = NkType_Tuple;

    ByteArray fp{};
    pushVal(fp, tclass);
    pushVal(fp, count);
    for (size_t i = 0; i < count; i++) {
        pushVal(fp, types[i * stride]);
    }

    return getTypeByFingerprint(std::move(fp), tclass, [=]() {
        return nkt_get_tuple(alloc, types, count, stride);
    });
}

nktype_t nkl_get_void() {
    auto const tclass = NkType_Void;

    ByteArray fp{};
    pushVal(fp, tclass);

    return getTypeByFingerprint(std::move(fp), tclass, [=]() {
        return nkt_get_void();
    });
}

nktype_t nkl_get_slice(NkAllocator alloc, nktype_t elem_type) {
    auto const tclass = NklType_Slice;

    ByteArray fp{};
    pushVal(fp, tclass);

    return getTypeByFingerprint(std::move(fp), tclass, [=]() {
        auto ptr_t = nkl_get_ptr(elem_type);
        auto u64_t = nkl_get_numeric(Uint64);
        nktype_t types[] = {ptr_t, u64_t};
        // TODO Duplicating tuple types with actual tuples
        return nkt_get_tuple(alloc, types, AR_SIZE(types), 1);
    });
}
