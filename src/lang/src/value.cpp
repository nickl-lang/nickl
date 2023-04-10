#include "nkl/lang/value.h"

#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>

#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/utils.h"
#include "nk/common/utils.hpp"
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
    EASY_FUNCTION(::profiler::colors::Green200);
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
    auto const tclass = NkType_Tuple;

    ByteArray fp{};
    pushVal(fp, NkTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, (size_t)0);

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

nkltype_t nkl_get_tuple(NkAllocator alloc, NklTypeArray types, size_t stride) {
    auto const tclass = NkType_Tuple;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, types.size);
    for (size_t i = 0; i < types.size; i++) {
        pushVal(fp, nklt_typeid(types.data[i * stride]));
    }

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        auto layout = nk_calcTupleLayout((nktype_t const *)types.data, types.size, alloc, stride);
        return &s_types.emplace_back(NklType{
            .vm_type = *nkl_get_vm_tuple(alloc, (nktype_t const *)types.data, types.size, stride),
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
    auto const tclass = NkType_Tuple;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, (size_t)0);

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        return &s_types.emplace_back(NklType{
            .vm_type = *nkl_get_vm_void(),
            .as{},
            .tclass = tclass,
            .id = s_next_id++,
        });
    });
}

nkltype_t nkl_get_typeref() {
    auto const tclass = NklType_Typeref;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        auto const void_ptr_t = nkl_get_ptr(nkl_get_void());
        return &s_types.emplace_back(NklType{
            .vm_type = *tovmt(void_ptr_t),
            .as{},
            .tclass = tclass,
            .id = s_next_id++,
            .underlying_type = void_ptr_t,
        });
    });
}

nkltype_t nkl_get_any(NkAllocator alloc) {
    auto const tclass = NklType_Any;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        auto const data_t = nkl_get_ptr(nkl_get_void()); // TODO Using *void as data in any_t
        auto const type_t = nkl_get_typeref();
        NklField const fields[] = {
            {
                .name = cs2nkid("data"),
                .type = data_t,
            },
            {
                .name = cs2nkid("type"),
                .type = type_t,
            },
        };
        auto const underlying_type = nkl_get_struct(alloc, {fields, AR_SIZE(fields)});
        return &s_types.emplace_back(NklType{
            .vm_type = *tovmt(underlying_type),
            .as{},
            .tclass = tclass,
            .id = s_next_id++,
            .underlying_type = underlying_type,
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
        auto const ptr_t = nkl_get_ptr(elem_type);
        auto const u64_t = nkl_get_numeric(Uint64);
        NklField const fields[] = {
            {
                .name = cs2nkid("data"),
                .type = ptr_t,
            },
            {
                .name = cs2nkid("size"),
                .type = u64_t,
            },
        };
        auto const underlying_type = nkl_get_struct(alloc, {fields, AR_SIZE(fields)});
        return &s_types.emplace_back(NklType{
            .vm_type = *tovmt(underlying_type),
            .as{.slice{
                .target_type = elem_type,
                .is_const = is_const,
            }},
            .tclass = tclass,
            .id = s_next_id++,
            .underlying_type = underlying_type,
        });
    });
}

nkltype_t nkl_get_struct(NkAllocator alloc, NklFieldArray fields) {
    auto const tclass = NklType_Struct;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, fields.size);
    for (size_t i = 0; i < fields.size; i++) {
        pushVal(fp, fields.data[i].name);
        pushVal(fp, nklt_typeid(fields.data[i].type));
    }

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        auto fields_data = (NklField *)nk_allocate(alloc, fields.size * sizeof(NklField));
        std::memcpy(fields_data, fields.data, fields.size * sizeof(NklField));
        auto underlying_type = nkl_get_tuple(
            alloc, {&fields.data[0].type, fields.size}, sizeof(NklField) / sizeof(nkltype_t));
        return &s_types.emplace_back(NklType{
            .vm_type = *tovmt(underlying_type),
            .as{.strct{
                .fields{
                    .data = fields_data,
                    .size = fields.size,
                },
            }},
            .tclass = tclass,
            .id = s_next_id++,
            .underlying_type = underlying_type,
        });
    });
}

nkltype_t nkl_get_union(NkAllocator alloc, NklFieldArray fields) {
    auto const tclass = NklType_Union;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, fields.size);
    for (size_t i = 0; i < fields.size; i++) {
        pushVal(fp, fields.data[i].name);
        pushVal(fp, nklt_typeid(fields.data[i].type));
    }

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        auto fields_data = (NklField *)nk_allocate(alloc, fields.size * sizeof(NklField));
        std::memcpy(fields_data, fields.data, fields.size * sizeof(NklField));
        nkltype_t largest_type = nkl_get_void();
        uint8_t max_align = 0;
        for (size_t i = 0; i < fields.size; i++) {
            auto const type = fields.data[i].type;
            if (nklt_sizeof(type) > nklt_sizeof(largest_type)) {
                largest_type = type;
            }
            max_align = maxu(max_align, nklt_alignof(type));
        }
        auto vm_type = *tovmt(largest_type);
        vm_type.align = max_align;
        return &s_types.emplace_back(NklType{
            .vm_type = vm_type,
            .as{.strct{
                .fields{
                    .data = fields_data,
                    .size = fields.size,
                },
            }},
            .tclass = tclass,
            .id = s_next_id++,
        });
    });
}

nkltype_t nkl_get_enum(NkAllocator alloc, NklFieldArray fields) {
    auto const tclass = NklType_Enum;

    ByteArray fp{};
    pushVal(fp, NklTypeSubset);
    pushVal(fp, tclass);
    pushVal(fp, fields.size);
    for (size_t i = 0; i < fields.size; i++) {
        pushVal(fp, fields.data[i].name);
        pushVal(fp, nklt_typeid(fields.data[i].type));
    }

    return (nkltype_t)getTypeByFingerprint(std::move(fp), [=]() {
        auto const data_t = nkl_get_union(alloc, fields);
        auto const tag_t = nkl_get_numeric(Uint64);
        NklField const enum_fields[] = {
            {
                .name = cs2nkid("data"),
                .type = data_t,
            },
            {
                .name = cs2nkid("tag"),
                .type = tag_t,
            },
        };
        auto const underlying_type = nkl_get_struct(alloc, {enum_fields, AR_SIZE(enum_fields)});
        return &s_types.emplace_back(NklType{
            .vm_type = *tovmt(underlying_type),
            .as{},
            .tclass = tclass,
            .id = s_next_id++,
            .underlying_type = underlying_type,
        });
    });
}

void nklt_inspect(nkltype_t type, NkStringBuilder sb) {
    switch (nklt_tclass(type)) {
    case NklType_Typeref:
        nksb_printf(sb, "type_t");
        break;

    case NklType_Any:
        nksb_printf(sb, "any_t");
        break;

    case NklType_Slice:
        nksb_printf(sb, "[]");
        nklt_inspect(type->as.slice.target_type, sb);
        break;

    case NklType_Struct: // TODO Struct type inspect not implemented
        nksb_printf(sb, "struct");
        break;

    case NklType_Union: // TODO Union type inspect not implemented
        nksb_printf(sb, "union");
        break;

    case NklType_Enum: // TODO Enum type inspect not implemented
        nksb_printf(sb, "enum");
        break;

    default:
        nkt_inspect(tovmt(type), sb);
        break;
    }
}

void nklval_inspect(nklval_t val, NkStringBuilder sb) {
    switch (nklval_tclass(val)) {
    case NklType_Any:    // TODO any_t value inspect not implemented
    case NklType_Slice:  // TODO Slice value inspect not implemented
    case NklType_Struct: // TODO Struct value inspect not implemented
    case NklType_Union:  // TODO Union value inspect not implemented
    case NklType_Enum:   // TODO Enum value inspect not implemented

    case NklType_Typeref:
        nklt_inspect(nklval_as(nkltype_t, val), sb);
        break;

    default:
        nkval_inspect(tovmv(val), sb);
        break;
    }
}

size_t nklt_struct_index(nkltype_t type, nkid name) {
    for (size_t i = 0; i < type->as.strct.fields.size; i++) {
        if (name == type->as.strct.fields.data[i].name) {
            return i;
        }
    }
    return -1;
}

void nklval_fn_invoke(nklval_t fn, nklval_t ret, nklval_t args) {
    return nkval_fn_invoke(tovmv(fn), tovmv(ret), tovmv(args));
}

size_t nklval_array_size(nklval_t self) {
    return nkval_array_size(tovmv(self));
}

nklval_t nklval_array_at(nklval_t self, size_t i) {
    return fromvmv(nkval_array_at(tovmv(self), i));
}

size_t nklval_tuple_size(nklval_t self) {
    return nkval_tuple_size(tovmv(self));
}

nklval_t nklval_tuple_at(nklval_t self, size_t i) {
    return fromvmv(nkval_tuple_at(tovmv(self), i));
}

nklval_t nklval_struct_at(nklval_t val, nkid name) {
    auto const type = nklval_typeof(val);
    auto const i = nklt_struct_index(type, name);
    return i == -1ull ? nklval_undefined() : nklval_tuple_at(val, i);
}
