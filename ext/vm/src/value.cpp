#include "nk/vm/value.hpp"

#include <cassert>
#include <cstring>

#include "nk/common/arena.hpp"
#include "nk/common/hashmap.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"

namespace nk {
namespace vm {

namespace {

struct ByteArray {
    size_t size;
    uint8_t const *data;
};

struct ByteArrayHashMapContext {
    static hash_t hash(ByteArray key) {
        return hash_array(key.data, key.data + key.size);
    }

    static bool equal_to(ByteArray lhs, ByteArray rhs) {
        return lhs.size == rhs.size && std::memcmp(lhs.data, rhs.data, lhs.size) == 0;
    }
};

ArenaAllocator s_typearena;
HashMap<ByteArray, Type *, ByteArrayHashMapContext> s_typemap;
size_t s_next_type_id = 1;

#define INIT_CAPACITY 64

struct _FpBase {
    typeclassid_t id;
};

Type **_findType(ByteArray fp) {
    return s_typemap.find(fp);
}

Type **_storeType(ByteArray fp) {
    return &s_typemap.insert(fp);
}

struct _TypeQueryRes {
    Type *type;
    bool inserted;
};

_TypeQueryRes _getType(ByteArray fp) {
    Type **found_type = _findType(fp);
    if (found_type) {
        return _TypeQueryRes{*found_type, false};
    }
    uint8_t *fp_copy_data = s_typearena.alloc<uint8_t>(fp.size);
    ByteArray fp_copy = {fp.size, fp_copy_data};
    std::memcpy(fp_copy_data, fp.data, fp.size);
    Type *type = s_typearena.alloc<Type>();
    *type = Type{
        .as = {{0}},
        .id = s_next_type_id++,
        .size = 0,
        .alignment = 0,
        .typeclass_id = ((_FpBase *)fp.data)->id};
    return _TypeQueryRes{*_storeType(fp_copy) = type, true};
}

struct _TupleLayout {
    TupleElemInfoArray info_ar;
    size_t size;
    size_t alignment;
};

_TupleLayout _calcTupleLayout(TypeArray types, size_t stride) {
    size_t alignment = 0;
    size_t offset = 0;

    TupleElemInfoArray const info_ar{s_typearena.alloc<TupleElemInfo>(types.size), types.size};

    for (size_t i = 0; i < types.size; i++) {
        type_t const type = types.data[i * stride];

        alignment = maxu(alignment, type->alignment);

        offset = roundUpSafe(offset, type->alignment);
        info_ar.data[i] = TupleElemInfo{type, offset};
        offset += type->size;
    }

    return _TupleLayout{info_ar, roundUpSafe(offset, alignment), alignment};
}

string _typeName(Allocator *allocator, type_t type) {
    switch (type->typeclass_id) {
    case Type_Array: {
        string s = _typeName(allocator, type->as.arr.elem_type);
        return string_format(
            allocator, "Array(%.*s, %lu)", s.size, s.data, type->as.arr.elem_count);
    }
    case Type_Numeric:
        switch (type->as.num.value_type) {
        case Int0:
            return cstr_to_str("i0");
        case Int1:
            return cstr_to_str("i1");
        case Int8:
            return cstr_to_str("i8");
        case Int16:
            return cstr_to_str("i16");
        case Int32:
            return cstr_to_str("i32");
        case Int64:
            return cstr_to_str("i64");
        case Uint0:
            return cstr_to_str("u0");
        case Uint1:
            return cstr_to_str("u1");
        case Uint8:
            return cstr_to_str("u8");
        case Uint16:
            return cstr_to_str("u16");
        case Uint32:
            return cstr_to_str("u32");
        case Uint64:
            return cstr_to_str("u64");
        case Float32:
            return cstr_to_str("f32");
        case Float64:
            return cstr_to_str("f64");
        default:
            assert(!"unreachable");
            return string{nullptr, 0};
        }
    case Type_Ptr: {
        string s = _typeName(allocator, type->as.ptr.target_type);
        return string_format(allocator, "Ptr(%.*s)", s.size, s.data);
    }
    case Type_Typeref:
        return cstr_to_str("type");
    case Type_Void:
        return cstr_to_str("void");
    case Type_Tuple: {
        string s = cstr_to_str("Tuple(");
        TupleElemInfoArray const info = type->as.tuple.elems;
        for (size_t i = 0; i < info.size; i++) {
            char const *fmt = i ? "%.*s, %.*s" : "%.*s%.*s";
            string ts = _typeName(allocator, info.data[i].type);
            s = string_format(allocator, fmt, s.size, s.data, ts.size, ts.data);
        }
        s = string_format(allocator, "%.*s)", s.size, s.data);
        return s;
    }
    case Type_Fn: {
        string s = cstr_to_str("Fn((");
        type_t const params = type->as.fn.args_t;
        for (size_t i = 0; i < params->as.tuple.elems.size; i++) {
            char const *fmt = i ? "%.*s, %.*s" : "%.*s%.*s";
            string const ts = _typeName(allocator, params->as.tuple.elems.data[i].type);
            s = string_format(allocator, fmt, s.size, s.data, ts.size, ts.data);
        }
        string const rts = _typeName(allocator, type->as.fn.ret_t);
        s = string_format(allocator, "%.*s), %.*s)", s.size, s.data, rts.size, rts.data);
        return s;
    }
    default:
        return string_format(allocator, "type{id=%lu}", type->id);
    }
}

} // namespace

void types_init() {
    s_typearena.init();
    s_typemap.init(100);

    s_next_type_id = 1;
}

void types_deinit() {
    s_typemap.deinit();
    s_typearena.deinit();
}

type_t type_get_array(type_t elem_type, size_t elem_count) {
    struct {
        _FpBase base;
        typeid_t elem_type;
        size_t elem_count;
    } fp;
    memset(&fp, 0, sizeof(fp));
    fp.base.id = Type_Array;
    fp.elem_type = elem_type->id;
    fp.elem_count = elem_count;
    _TypeQueryRes res = _getType({sizeof(fp), (uint8_t *)&fp});
    if (res.inserted) {
        res.type->size = elem_count * elem_type->size;
        res.type->alignment = elem_type->alignment;
        res.type->as.arr.elem_type = elem_type;
        res.type->as.arr.elem_count = elem_count;
    }
    return res.type;
}

type_t type_get_fn(type_t ret_t, type_t params_t, size_t decl_id, void *body_ptr, void *closure) {
    struct {
        _FpBase base;
        size_t decl_id;
        typeid_t ret_t;
        typeid_t params_t;
    } fp;
    std::memset(&fp, 0, sizeof(fp));
    fp.base.id = Type_Fn;
    fp.decl_id = decl_id;
    fp.ret_t = ret_t->id;
    fp.params_t = params_t->id;
    _TypeQueryRes res = _getType({sizeof(fp), (uint8_t *)&fp});
    if (res.inserted) {
        res.type->size = 0;
        res.type->alignment = 1;
        res.type->as.fn.ret_t = ret_t;
        res.type->as.fn.args_t = params_t;
        res.type->as.fn.body.native_ptr = body_ptr;
        res.type->as.fn.closure = closure;
    }
    return res.type;
}

type_t type_get_numeric(ENumericValueType value_type) {
    struct {
        _FpBase base;
        ENumericValueType value_type;
    } fp;
    std::memset(&fp, 0, sizeof(fp));
    fp.base.id = Type_Numeric;
    fp.value_type = value_type;
    _TypeQueryRes res = _getType({sizeof(fp), (uint8_t *)&fp});
    if (res.inserted) {
        size_t const size = value_type & NUM_TYPE_SIZE_MASK;
        res.type->size = size;
        res.type->alignment = size;
        res.type->as.num.value_type = value_type;
    }
    return res.type;
}

type_t type_get_ptr(type_t target_type) {
    struct {
        _FpBase base;
        typeid_t target_type;
    } fp;
    std::memset(&fp, 0, sizeof(fp));
    fp.base.id = Type_Ptr;
    fp.target_type = target_type->id;
    _TypeQueryRes res = _getType({sizeof(fp), (uint8_t *)&fp});
    if (res.inserted) {
        res.type->size = sizeof(void *);
        res.type->alignment = alignof(void *);
        res.type->as.ptr.target_type = target_type;
    }
    return res.type;
}

type_t type_get_tuple(Allocator *tmp_allocator, TypeArray types) {
    struct Fp {
        _FpBase base;
        size_t type_count;
    };
    size_t const fp_size = arrayWithHeaderSize<Fp, typeid_t>(types.size);
    Fp *fp = (Fp *)tmp_allocator->alloc_aligned(fp_size, alignof(Fp));
    auto fp_types = arrayWithHeaderData<Fp, typeid_t>(fp);
    std::memset(fp, 0, fp_size);
    fp->base.id = Type_Tuple;
    fp->type_count = types.size;
    for (size_t i = 0; i < types.size; i++) {
        fp_types[i] = types.data[i]->id;
    }
    _TypeQueryRes res = _getType({fp_size, (uint8_t *)fp});
    if (res.inserted) {
        _TupleLayout layout = _calcTupleLayout({types.data, types.size}, 1);

        res.type->size = layout.size;
        res.type->alignment = layout.alignment;
        res.type->as.tuple.elems = layout.info_ar;
    }
    return res.type;
}

type_t type_get_typeref() {
    _FpBase fp;
    std::memset(&fp, 0, sizeof(fp));
    fp.id = Type_Typeref;
    _TypeQueryRes res = _getType({sizeof(fp), (uint8_t *)&fp});
    if (res.inserted) {
        res.type->size = sizeof(size_t);
        res.type->alignment = alignof(type_t);
    }
    return res.type;
}

type_t type_get_void() {
    _FpBase fp;
    std::memset(&fp, 0, sizeof(fp));
    fp.id = Type_Void;
    _TypeQueryRes res = _getType({sizeof(fp), (uint8_t *)&fp});
    if (res.inserted) {
        res.type->size = 0;
        res.type->alignment = 1;
    }
    return res.type;
}

string type_name(Allocator *allocator, type_t type) {
    // TODO rewrite with stringstream

    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    size_t const frame = tmp_arena._seq.size;
    string tmp_str = _typeName(&tmp_arena, type);
    tmp_arena._seq.pop(tmp_arena._seq.size - frame);
    char *data = allocator->alloc<char>(tmp_str.size);
    std::memcpy(data, tmp_str.data, tmp_str.size);

    return string{data, tmp_str.size};
}

bool val_isTrue(value_t val) {
    switch (val.type->typeclass_id) {
    case Type_Numeric:
    case Type_Ptr:
    case Type_Typeref:
        // TODO val_isTrue not implemented
        return false;
    default:
        return true;
    }
}

} // namespace vm
} // namespace nk
