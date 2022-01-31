#include "nk/vm/value.hpp"

#include <cstring>
#include <sstream>

#include "nk/common/arena.hpp"
#include "nk/common/hashmap.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"

namespace nk {
namespace vm {

namespace {

struct ByteArray {
    uint8_t const *data;
    size_t size;
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
    ByteArray fp_copy = {fp_copy_data, fp.size};
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

void _typeName(type_t type, std::ostringstream &ss) {
    switch (type->typeclass_id) {
    case Type_Array:
        ss << "array{";
        _typeName(type->as.arr.elem_type, ss);
        ss << ", " << type->as.arr.elem_count << "}";
        break;
    case Type_Numeric:
        switch (type->as.num.value_type) {
        case Int0:
        case Int8:
        case Int16:
        case Int32:
        case Int64:
            ss << "i";
            break;
        case Int1:
            ss << "i1";
            return;
        case Uint0:
        case Uint8:
        case Uint16:
        case Uint32:
        case Uint64:
            ss << "u";
            break;
        case Uint1:
            ss << "u1";
            return;
        case Float32:
        case Float64:
            ss << "f";
            break;
        default:
            assert(!"unreachable");
            break;
        }
        ss << (type->as.num.value_type & NUM_TYPE_SIZE_MASK) * 8;
        break;
    case Type_Ptr:
        ss << "ptr{";
        _typeName(type->as.ptr.target_type, ss);
        ss << "}";
        break;
    case Type_Typeref:
        ss << "type";
        break;
    case Type_Void:
        ss << "void";
        break;
    case Type_Tuple: {
        ss << "tuple{";
        TupleElemInfoArray const info = type->as.tuple.elems;
        for (size_t i = 0; i < info.size; i++) {
            if (i) {
                ss << ", ";
            }
            _typeName(info.data[i].type, ss);
        }
        ss << "}";
        break;
    }
    case Type_Fn: {
        ss << "fn{(";
        type_t const params = type->as.fn.args_t;
        for (size_t i = 0; i < params->as.tuple.elems.size; i++) {
            if (i) {
                ss << ", ";
            }
            _typeName(params->as.tuple.elems.data[i].type, ss);
        }
        ss << "), ";
        _typeName(type->as.fn.ret_t, ss);
        ss << "}";
        break;
    }
    default:
        ss << "type{id=" << type->id << "}";
        break;
    }
}

void _valInspect(value_t val, std::ostringstream &ss) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    switch (val_typeid(val)) {
    case Type_Array:
        ss << "[";
        for (size_t i = 0; i < val_array_size(val); i++) {
            if (i) {
                ss << " ";
            }
            _valInspect(val_array_at(val, i), ss);
            ss << ",";
        }
        ss << "]";
        break;
    case Type_Numeric:
        val_numeric_visit(val, [&](auto const &val) {
            ss << val;
        });
        break;
    case Type_Ptr:
        ss << val_data(val);
        break;
    case Type_Tuple:
        ss << "(";
        for (size_t i = 0; i < val_tuple_size(val); i++) {
            if (i) {
                ss << " ";
            }
            _valInspect(val_tuple_at(val, i), ss);
            ss << ",";
        }
        ss << ")";
        break;
    case Type_Typeref:
        ss << type_name(&tmp_arena, val_typeof(val)).view();
        break;
    case Type_Void:
        ss << "void{}";
        break;
    case Type_Fn:
        // TODO inspect function
    default:
        ss << "value{data=" << val_data(val)
           << ", type=" << type_name(&tmp_arena, val_typeof(val)).view() << "}";
        break;
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
    } fp = {};
    fp.base.id = Type_Array;
    fp.elem_type = elem_type->id;
    fp.elem_count = elem_count;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = elem_count * elem_type->size;
        res.type->alignment = elem_type->alignment;
        res.type->as.arr.elem_type = elem_type;
        res.type->as.arr.elem_count = elem_count;
    }
    return res.type;
}

type_t type_get_fn(type_t ret_t, type_t args_t, size_t decl_id, FuncPtr body_ptr, void *closure) {
    struct {
        _FpBase base;
        size_t decl_id;
        typeid_t ret_t;
        typeid_t args_t;
        FuncPtr body_ptr;
    } fp = {};
    fp.base.id = Type_Fn;
    fp.decl_id = decl_id;
    fp.ret_t = ret_t->id;
    fp.args_t = args_t->id;
    fp.body_ptr = body_ptr;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = 0;
        res.type->alignment = 1;
        res.type->as.fn.ret_t = ret_t;
        res.type->as.fn.args_t = args_t;
        res.type->as.fn.body = body_ptr;
        res.type->as.fn.closure = closure;
    }
    return res.type;
}

type_t type_get_numeric(ENumericValueType value_type) {
    struct {
        _FpBase base;
        ENumericValueType value_type;
    } fp = {};
    fp.base.id = Type_Numeric;
    fp.value_type = value_type;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
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
    } fp = {};
    fp.base.id = Type_Ptr;
    fp.target_type = target_type->id;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
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
    _TypeQueryRes res = _getType({(uint8_t *)fp, fp_size});
    if (res.inserted) {
        _TupleLayout layout = _calcTupleLayout({types.data, types.size}, 1);

        res.type->size = layout.size;
        res.type->alignment = layout.alignment;
        res.type->as.tuple.elems = layout.info_ar;
    }
    return res.type;
}

type_t type_get_typeref() {
    _FpBase fp = {};
    fp.id = Type_Typeref;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = sizeof(size_t);
        res.type->alignment = alignof(type_t);
    }
    return res.type;
}

type_t type_get_void() {
    _FpBase fp = {};
    fp.id = Type_Void;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = 0;
        res.type->alignment = 1;
    }
    return res.type;
}

string type_name(Allocator *allocator, type_t type) {
    std::ostringstream ss;
    _typeName(type, ss);
    auto str = ss.str();

    char *data = (char *)allocator->alloc(str.size());
    std::memcpy(data, str.data(), str.size());

    return string{data, str.size()};
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

string val_inspect(Allocator *allocator, value_t val) {
    std::ostringstream ss;
    _valInspect(val, ss);
    auto str = ss.str();

    char *data = (char *)allocator->alloc(str.size());
    std::memcpy(data, str.data(), str.size());

    return string{data, str.size()};
}

size_t val_tuple_size(value_t self) {
    return val_typeof(self)->as.tuple.elems.size;
}

value_t val_tuple_at(value_t self, size_t i) {
    assert(i < val_tuple_size(self) && "tuple index out of range");
    auto const type = val_typeof(self);
    return {
        ((uint8_t *)val_data(self)) + type->as.tuple.elems.data[i].offset,
        type->as.tuple.elems.data[i].type};
}

size_t type_tuple_offset(type_t tuple_t, size_t i) {
    assert(i < tuple_t->as.tuple.elems.size && "tuple index out of range");
    return tuple_t->as.tuple.elems.data[i].offset;
}

size_t val_array_size(value_t self) {
    return val_typeof(self)->as.arr.elem_count;
}

value_t val_array_at(value_t self, size_t i) {
    assert(i < val_array_size(self) && "array index out of range");
    auto const type = val_typeof(self);
    return {((uint8_t *)val_data(self)) + type->as.arr.elem_type->size * i, type->as.arr.elem_type};
}

void val_fn_invoke(type_t self, value_t ret, value_t args) {
    self->as.fn.body(self, ret, args);
}

} // namespace vm
} // namespace nk
