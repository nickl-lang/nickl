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

std::deque<NkType> s_vm_types;
std::deque<NklType> s_types;
std::map<ByteArray, nktype_t> s_typemap;
std::recursive_mutex s_mtx;
nkl_typeid_t s_next_id = 1;

enum ETypeSubset {
    NkTypeSubset,
    NklTypeSubset,
};

template <class F>
nktype_t getTypeByFingerprint(ByteArray fp, F const &create_type) {
    EASY_FUNCTION(); // TODO Choose color
    NK_LOG_TRC(__func__);

    std::lock_guard lk{s_mtx};

    auto it = s_typemap.find(fp);
    if (it == s_typemap.end()) {
        bool inserted = false;
        auto type = (nktype_t)create_type();
        std::tie(it, inserted) = s_typemap.emplace(std::move(fp), type);
        assert(inserted && "type duplication");
    }
    return it->second;
}

template <class T>
void pushVal(ByteArray &ar, T v) {
    ar.insert(ar.end(), (uint8_t *)&v, (uint8_t *)&v + sizeof(v));
}

nktype_t nkl_get_vm_array(nktype_t elem_type, size_t elem_count) {
    auto const tclass = NkType_Array;

    ByteArray fp{};
    pushVal(fp, NkTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, nkt_typeid(elem_type));
    pushVal(fp, elem_count);

    return getTypeByFingerprint(std::move(fp), [=]() {
        return &s_vm_types.emplace_back(nkt_get_array(elem_type, elem_count));
    });
}

nktype_t nkl_get_vm_fn(NktFnInfo info) {
    auto const tclass = NkType_Fn;

    ByteArray fp{};
    pushVal(fp, NkTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, nkt_typeid(info.ret_t));
    pushVal(fp, nkt_typeid(info.args_t));
    pushVal(fp, info.call_conv);
    pushVal(fp, info.is_variadic);

    return getTypeByFingerprint(std::move(fp), [=]() {
        return &s_vm_types.emplace_back(nkt_get_fn(NktFnInfo{
            .ret_t = info.ret_t,
            .args_t = info.args_t,
            .call_conv = info.call_conv,
            .is_variadic = info.is_variadic,
        }));
    });
}

nktype_t nkl_get_vm_numeric(NkNumericValueType value_type) {
    auto const tclass = NkType_Numeric;

    ByteArray fp{};
    pushVal(fp, NkTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, value_type);

    return getTypeByFingerprint(std::move(fp), [=]() {
        return &s_vm_types.emplace_back(nkt_get_numeric(value_type));
    });
}

nktype_t nkl_get_vm_ptr(nktype_t target_type) {
    auto const tclass = NkType_Ptr;

    ByteArray fp{};
    pushVal(fp, NkTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, nkt_typeid(target_type));

    return getTypeByFingerprint(std::move(fp), [=]() {
        return &s_vm_types.emplace_back(nkt_get_ptr(target_type));
    });
}

nktype_t nkl_get_vm_tuple(NkAllocator alloc, nktype_t const *types, size_t count, size_t stride) {
    auto const tclass = NkType_Tuple;

    ByteArray fp{};
    pushVal(fp, NkTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, count);
    for (size_t i = 0; i < count; i++) {
        pushVal(fp, nkt_typeid(types[i * stride]));
    }

    return getTypeByFingerprint(std::move(fp), [=]() {
        return &s_vm_types.emplace_back(nkt_get_tuple(alloc, types, count, stride));
    });
}

nktype_t nkl_get_vm_void() {
    auto const tclass = NkType_Void;

    ByteArray fp{};
    pushVal(fp, NkTypeSubset);
    pushVal(fp, tclass);

    return getTypeByFingerprint(std::move(fp), [=]() {
        return &s_vm_types.emplace_back(nkt_get_void());
    });
}

} // namespace

void nkl_types_clean() {
    std::lock_guard lk{s_mtx};

    s_next_id = 1;

    s_vm_types.clear();
    s_types.clear();
    s_typemap.clear();
}

nkltype_t nkl_get_array(nkltype_t elem_type, size_t elem_count) {
    auto const tclass = NkType_Array;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, nklt_typeid(elem_type));
    pushVal(fp, elem_count);

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        return &s_types.emplace_back(NklType{
            .vm_type = *nkl_get_vm_array(tovmt(elem_type), elem_count),
            .as{.arr{
                .elem_type = elem_type,
                .elem_count = elem_count,
            }},
            .tclass = tclass,
            .id = s_next_id++,
        });
    });
}

nkltype_t nkl_get_fn(NkltFnInfo info) {
    auto const tclass = NkType_Fn;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, nklt_typeid(info.ret_t));
    pushVal(fp, nklt_typeid(info.args_t));
    pushVal(fp, info.call_conv);
    pushVal(fp, info.is_variadic);

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        return &s_types.emplace_back(NklType{
            .vm_type = *nkl_get_vm_fn(tovmf(info)),
            .as{.fn{
                .ret_t = info.ret_t,
                .args_t = info.args_t,
                .call_conv = info.call_conv,
                .is_variadic = info.is_variadic,
            }},
            .tclass = tclass,
            .id = s_next_id++,
        });
    });
}

nkltype_t nkl_get_numeric(NkNumericValueType value_type) {
    auto const tclass = NkType_Numeric;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, value_type);

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        return &s_types.emplace_back(NklType{
            .vm_type = *nkl_get_vm_numeric(value_type),
            .as{.num{
                .value_type = value_type,
            }},
            .tclass = tclass,
            .id = s_next_id++,
        });
    });
}

nkltype_t nkl_get_ptr(nkltype_t target_type, bool is_const) {
    auto const tclass = NkType_Ptr;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, nklt_typeid(target_type));
    pushVal(fp, is_const);

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        return &s_types.emplace_back(NklType{
            .vm_type = *nkl_get_vm_ptr(tovmt(target_type)),
            .as{.ptr{
                .target_type = target_type,
                .is_const = is_const,
            }},
            .tclass = tclass,
            .id = s_next_id++,
        });
    });
}

nkltype_t nkl_get_tuple(NkAllocator alloc, nkltype_t const *types, size_t count, size_t stride) {
    auto const tclass = NkType_Tuple;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, count);
    for (size_t i = 0; i < count; i++) {
        pushVal(fp, nklt_typeid(types[i * stride]));
    }

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        auto layout = nk_calcTupleLayout((nktype_t const *)types, count, alloc, stride);
        return &s_types.emplace_back(NklType{
            .vm_type = *nkl_get_vm_tuple(alloc, (nktype_t const *)types, count, stride),
            .as{.tuple{
                .elems{
                    .data = (NklTupleElemInfo *)layout.info_ar.data,
                    .size = layout.info_ar.size,
                },
            }},
            .tclass = tclass,
            .id = s_next_id++,
        });
    });
}

nkltype_t nkl_get_void() {
    auto const tclass = NkType_Void;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        return &s_types.emplace_back(NklType{
            .vm_type = *nkl_get_vm_void(),
            .as{},
            .tclass = tclass,
            .id = s_next_id++,
        });
    });
}

nkltype_t nkl_get_slice(NkAllocator alloc, nkltype_t elem_type, bool is_const) {
    auto const tclass = NklType_Slice;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, nklt_typeid(elem_type));
    pushVal(fp, is_const);

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        auto ptr_t = nkl_get_vm_ptr(tovmt(elem_type));
        auto u64_t = nkl_get_vm_numeric(Uint64);
        nktype_t types[] = {ptr_t, u64_t};
        return &s_types.emplace_back(NklType{
            .vm_type = *nkl_get_vm_tuple(alloc, types, AR_SIZE(types), 1),
            .as{.slice{
                .target_type = elem_type,
                .is_const = is_const,
            }},
            .tclass = tclass,
            .id = s_next_id++,
        });
    });
}

void nklt_inspect(nkltype_t type, NkStringBuilder sb) {
    // TODO nklt_inspect unfinished
    nkt_inspect(tovmt(type), sb);
}

void nklval_inspect(nklval_t val, NkStringBuilder sb) {
    // TODO nklval_inspect unfinished
    nkval_inspect(tovmv(val), sb);
}
