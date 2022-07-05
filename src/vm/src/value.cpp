#include "nk/vm/value.hpp"

#include <cstring>

#include "native_fn_adapter.hpp"
#include "nk/ds/hashmap.hpp"
#include "nk/mem/stack_allocator.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nk/utils/utils.hpp"

namespace nk {
namespace vm {

namespace {

LOG_USE_SCOPE(nk::vm::value);

using ByteArray = Slice<uint8_t const>;

StackAllocator s_typearena;
HashMap<ByteArray, Type *> s_typemap;
size_t s_next_type_id = 1;

#define INIT_CAPACITY 64

struct _FpBase {
    typeclassid_t id;
};

Type **_findType(ByteArray fp) {
    return s_typemap.find(fp);
}

Type **_storeType(ByteArray fp, Type *type) {
    return &s_typemap.insert(fp, type);
}

struct _TypeQueryRes {
    Type *type;
    bool inserted;
};

_TypeQueryRes _getType(ByteArray fp) {
    EASY_FUNCTION(profiler::colors::Green200)

    Type **found_type = _findType(fp);
    if (found_type) {
        return _TypeQueryRes{*found_type, false};
    }
    auto fp_copy = s_typearena.alloc<uint8_t>(fp.size);
    fp.copy(fp_copy);
    Type *type = s_typearena.alloc<Type>();
    *type = Type{
        .as = {{0}},
        .native_handle = nullptr,
        .id = s_next_type_id++,
        .size = 0,
        .alignment = 0,
        .typeclass_id = ((_FpBase *)fp.data)->id};
    return _TypeQueryRes{*_storeType(fp_copy, type), true};
}

void _typeName(type_t type, StringBuilder &sb) {
    switch (type->typeclass_id) {
    case Type_Array:
        sb << "array{";
        _typeName(type->as.arr.elem_type, sb);
        sb << ", " << type->as.arr.elem_count << "}";
        break;
    case Type_Numeric:
        switch (type->as.num.value_type) {
        case Int8:
        case Int16:
        case Int32:
        case Int64:
            sb << "i";
            break;
        case Uint8:
        case Uint16:
        case Uint32:
        case Uint64:
            sb << "u";
            break;
        case Float32:
        case Float64:
            sb << "f";
            break;
        default:
            assert(!"unreachable");
            break;
        }
        sb << NUM_TYPE_SIZE(type->as.num.value_type) * 8;
        break;
    case Type_Ptr:
        sb << "ptr{";
        _typeName(type->as.ptr.target_type, sb);
        sb << "}";
        break;
    case Type_Void:
        sb << "void";
        break;
    case Type_Tuple: {
        sb << "tuple{";
        for (size_t i = 0; i < type_tuple_size(type); i++) {
            if (i) {
                sb << ", ";
            }
            _typeName(type_tuple_typeAt(type, i), sb);
        }
        sb << "}";
        break;
    }
    case Type_Fn: {
        sb << "fn{(";
        type_t const params = type->as.fn.args_t;
        for (size_t i = 0; i < type_tuple_size(params); i++) {
            if (i) {
                sb << ", ";
            }
            _typeName(type_tuple_typeAt(params, i), sb);
        }
        sb << "), ";
        _typeName(type->as.fn.ret_t, sb);
        sb << "}";
        break;
    }
    default:
        sb << "type{id=" << type->id << "}";
        break;
    }
}

void _valInspect(value_t val, StringBuilder &sb) {
    switch (val_typeclassid(val)) {
    case Type_Array:
        sb << "[";
        for (size_t i = 0; i < val_array_size(val); i++) {
            if (i) {
                sb << " ";
            }
            _valInspect(val_array_at(val, i), sb);
            sb << ",";
        }
        sb << "]";
        break;
    case Type_Numeric:
        switch (val_typeof(val)->as.num.value_type) {
        case Int8:
            sb << (int)val_as(int8_t, val);
            break;
        case Uint8:
            sb << (unsigned)val_as(uint8_t, val);
            break;
        case Int16:
            sb << val_as(int16_t, val);
            break;
        case Uint16:
            sb << val_as(uint16_t, val);
            break;
        case Int32:
            sb << val_as(int32_t, val);
            break;
        case Uint32:
            sb << val_as(uint32_t, val);
            break;
        case Int64:
            sb << val_as(int64_t, val);
            break;
        case Uint64:
            sb << val_as(uint64_t, val);
            break;
        case Float32:
            sb << val_as(float, val);
            break;
        case Float64:
            sb << val_as(double, val);
            break;
        default:
            assert(!"unreachable");
            break;
        }
        break;
    case Type_Ptr: {
        type_t target_type = val_typeof(val)->as.ptr.target_type;
        if (target_type->typeclass_id == Type_Array) {
            type_t elem_type = target_type->as.arr.elem_type;
            size_t elem_count = target_type->as.arr.elem_count;
            if (elem_type->typeclass_id == Type_Numeric) {
                if (elem_type->as.num.value_type == Int8 || elem_type->as.num.value_type == Uint8) {
                    sb << '"';
                    string_escape(sb, {val_as(char const *, val), elem_count});
                    sb << '"';
                    break;
                }
            }
        }
        sb << val_data(val);
        break;
    }
    case Type_Tuple:
        sb << "(";
        for (size_t i = 0; i < val_tuple_size(val); i++) {
            if (i) {
                sb << " ";
            }
            _valInspect(val_tuple_at(val, i), sb);
            sb << ",";
        }
        sb << ")";
        break;
    case Type_Void:
        sb << "void{}";
        break;
    case Type_Fn:
        sb << "fn@" << (void *)val_typeof(val)->as.fn.body << "+" << val_typeof(val)->as.fn.closure;
        break;
    default: {
        sb << "value{data=" << val_data(val) << ", type=";
        type_name(val_typeof(val), sb);
        sb << "}";
        break;
    }
    }
}

} // namespace

TupleLayout calcTupleLayout(TypeArray types, Allocator &allocator, size_t stride) {
    size_t alignment = 0;
    size_t offset = 0;

    TupleElemInfoArray const info_ar{allocator.alloc<TupleElemInfo>(types.size), types.size};

    for (size_t i = 0; i < types.size; i++) {
        type_t const type = types[i * stride];

        alignment = maxu(alignment, type->alignment);

        offset = roundUpSafe(offset, type->alignment);
        info_ar[i] = TupleElemInfo{type, offset};
        offset += type->size;
    }

    return TupleLayout{info_ar, roundUpSafe(offset, alignment), alignment};
}

void types_init() {
    EASY_FUNCTION(profiler::colors::Green200)

    s_typemap.reserve(100);

    s_next_type_id = 1;
}

void types_deinit() {
    EASY_FUNCTION(profiler::colors::Green200)

    s_typemap.deinit();
    s_typearena.deinit();
}

type_t type_get_array(type_t elem_type, size_t elem_count) {
    EASY_FUNCTION(profiler::colors::Green200)

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
    EASY_FUNCTION(profiler::colors::Green200)

    struct {
        _FpBase base;
        size_t decl_id;
        typeid_t ret_t;
        typeid_t args_t;
        FuncPtr body_ptr;
        void *closure;
        bool is_native;
    } fp = {};
    fp.base.id = Type_Fn;
    fp.decl_id = decl_id;
    fp.ret_t = ret_t->id;
    fp.args_t = args_t->id;
    fp.body_ptr = body_ptr;
    fp.closure = closure;
    fp.is_native = false;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = 0;
        res.type->alignment = 1;
        res.type->as.fn.ret_t = ret_t;
        res.type->as.fn.args_t = args_t;
        res.type->as.fn.body = body_ptr;
        res.type->as.fn.closure = closure;
        res.type->as.fn.is_native = false;
    }
    return res.type;
}

type_t type_get_fn_native(
    type_t ret_t,
    type_t args_t,
    size_t decl_id,
    void *body_ptr,
    bool is_variadic) {
    EASY_FUNCTION(profiler::colors::Green200)

    struct {
        _FpBase base;
        size_t decl_id;
        typeid_t ret_t;
        typeid_t args_t;
        void *body_ptr;
        bool is_native;
        bool is_variadic;
    } fp = {};
    fp.base.id = Type_Fn;
    fp.decl_id = decl_id;
    fp.ret_t = ret_t->id;
    fp.args_t = args_t->id;
    fp.body_ptr = body_ptr;
    fp.is_native = true;
    fp.is_variadic = is_variadic;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = 0;
        res.type->alignment = 1;
        res.type->as.fn.ret_t = ret_t;
        res.type->as.fn.args_t = args_t;
        type_fn_prepareNativeInfo(
            s_typearena,
            body_ptr,
            type_tuple_size(args_t),
            is_variadic,
            res.type->as.fn.body,
            res.type->as.fn.closure);
        res.type->as.fn.is_native = true;
    }
    return res.type;
}

type_t type_get_numeric(ENumericValueType value_type) {
    EASY_FUNCTION(profiler::colors::Green200)

    struct {
        _FpBase base;
        ENumericValueType value_type;
    } fp = {};
    fp.base.id = Type_Numeric;
    fp.value_type = value_type;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        size_t const size = NUM_TYPE_SIZE(value_type);
        res.type->size = size;
        res.type->alignment = size;
        res.type->as.num.value_type = value_type;
    }
    return res.type;
}

type_t type_get_ptr(type_t target_type) {
    EASY_FUNCTION(profiler::colors::Green200)

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

type_t type_get_tuple(TypeArray types) {
    EASY_FUNCTION(profiler::colors::Green200)

    struct Fp {
        _FpBase base;
        size_t type_count;
    };
    size_t const fp_size = arrayWithHeaderSize<Fp, typeid_t>(types.size);
    Fp *fp = (Fp *)nk_platform_alloc_aligned(fp_size, alignof(Fp));
    defer {
        nk_platform_free_aligned(fp);
    };
    auto fp_types = arrayWithHeaderData<Fp, typeid_t>(fp);
    std::memset(fp, 0, fp_size);
    fp->base.id = Type_Tuple;
    fp->type_count = types.size;
    for (size_t i = 0; i < types.size; i++) {
        fp_types[i] = types[i]->id;
    }
    _TypeQueryRes res = _getType({(uint8_t *)fp, fp_size});
    if (res.inserted) {
        TupleLayout layout = calcTupleLayout(types, s_typearena);

        res.type->size = layout.size;
        res.type->alignment = layout.alignment;
        res.type->as.tuple.elems = layout.info_ar;
    }
    return res.type;
}

type_t type_get_void() {
    EASY_FUNCTION(profiler::colors::Green200)

    _FpBase fp = {};
    fp.id = Type_Void;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = 0;
        res.type->alignment = 1;
    }
    return res.type;
}

StringBuilder &type_name(type_t type, StringBuilder &sb) {
    EASY_FUNCTION(profiler::colors::Green200)

    _typeName(type, sb);
    return sb;
}

StringBuilder &val_inspect(value_t val, StringBuilder &sb) {
    EASY_FUNCTION(profiler::colors::Green200)

    _valInspect(val, sb);
    return sb;
}

size_t val_tuple_size(value_t self) {
    return val_typeof(self)->as.tuple.elems.size;
}

value_t val_tuple_at(value_t self, size_t i) {
    assert(i < val_tuple_size(self) && "tuple index out of range");
    auto const type = val_typeof(self);
    return {
        ((uint8_t *)val_data(self)) + type->as.tuple.elems[i].offset, type->as.tuple.elems[i].type};
}

size_t type_tuple_size(type_t tuple_t) {
    return tuple_t->as.tuple.elems.size;
}

type_t type_tuple_typeAt(type_t tuple_t, size_t i) {
    assert(i < type_tuple_size(tuple_t) && "tuple index out of range");
    return tuple_t->as.tuple.elems[i].type;
}

size_t type_tuple_offsetAt(type_t tuple_t, size_t i) {
    assert(i < tuple_t->as.tuple.elems.size && "tuple index out of range");
    return tuple_t->as.tuple.elems[i].offset;
}

size_t val_array_size(value_t self) {
    return val_typeof(self)->as.arr.elem_count;
}

value_t val_array_at(value_t self, size_t i) {
    assert(i < val_array_size(self) && "array index out of range");
    auto const type = val_typeof(self);
    return {((uint8_t *)val_data(self)) + type->as.arr.elem_type->size * i, type->as.arr.elem_type};
}

size_t type_array_size(type_t array_t) {
    return array_t->as.arr.elem_count;
}

type_t type_array_elemType(type_t array_t) {
    return array_t->as.arr.elem_type;
}

value_t val_ptr_deref(value_t self) {
    return {val_as(void *, self), val_typeof(self)->as.ptr.target_type};
}

void val_fn_invoke(type_t self, value_t ret, value_t args) {
    EASY_FUNCTION(profiler::colors::Green200)
    LOG_TRC(__func__);

    self->as.fn.body(self, ret, args);
}

} // namespace vm
} // namespace nk
