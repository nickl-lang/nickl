#include "nkl/core/value.hpp"

#include <cassert>
#include <cstring>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"

namespace {

using ByteArray = Slice<uint8_t const>;

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
    QualifierRecord qualifiers;
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
        .typeclass_id = ((_FpBase *)fp.data)->id,
        .qualifiers = 0};
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
        type_t const type = types[i * stride];

        alignment = maxu(alignment, type->alignment);

        offset = roundUpSafe(offset, type->alignment);
        info_ar[i] = TupleElemInfo{type, offset};
        offset += type->size;
    }

    return _TupleLayout{info_ar, roundUpSafe(offset, alignment), alignment};
}

string _typeName(Allocator *allocator, type_t type) {
    switch (type->typeclass_id) {
    case Type_Any:
        return cstr_to_str("any");
    case Type_Array: {
        string s = _typeName(allocator, type->as.arr_t.elem_type);
        return string_format(
            allocator, "Array(%.*s, %lu)", s.size, s.data, type->as.arr_t.elem_count);
    }
    case Type_ArrayPtr: {
        string s = _typeName(allocator, type->as.arr_ptr_t.elem_type);
        return string_format(allocator, "ArrayPtr(%.*s)", s.size, s.data);
    }
    case Type_Bool:
        return cstr_to_str("bool");
    case Type_Numeric:
        switch (type->as.num_t.value_type) {
        case Int8:
            return cstr_to_str("i8");
        case Int16:
            return cstr_to_str("i16");
        case Int32:
            return cstr_to_str("i32");
        case Int64:
            return cstr_to_str("i64");
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
        string s = _typeName(allocator, type->as.ptr_t.target_type);
        return string_format(allocator, "Ptr(%.*s)", s.size, s.data);
    }
    case Type_String:
        return cstr_to_str("string");
    case Type_Symbol:
        return cstr_to_str("symbol");
    case Type_Typeref:
        return cstr_to_str("type");
    case Type_Void:
        return cstr_to_str("void");
    case Type_Tuple: {
        string s = cstr_to_str("Tuple(");
        TupleElemInfoArray const info = type->as.tuple_t.types;
        for (size_t i = 0; i < info.size; i++) {
            char const *fmt = i ? "%.*s, %.*s" : "%.*s%.*s";
            string ts = _typeName(allocator, info[i].type);
            s = string_format(allocator, fmt, s.size, s.data, ts.size, ts.data);
        }
        s = string_format(allocator, "%.*s)", s.size, s.data);
        return s;
    }
    case Type_Struct: {
        string s = cstr_to_str("struct {");
        TupleElemInfoArray const info = type->as.struct_t.types;
        auto const fields = type->as.struct_t.fields;
        for (size_t i = 0; i < info.size; i++) {
            string name = id_to_str(fields[i]);
            string const ts = _typeName(allocator, info[i].type);
            char const *fmt = i ? "%.*s, %.*s: %.*s" : "%.*s%.*s: %.*s";
            s = string_format(
                allocator, fmt, s.size, s.data, name.size, name.data, ts.size, ts.data);
        }
        s = string_format(allocator, "%.*s}", s.size, s.data);
        return s;
    }
    case Type_Fn: {
        string s = cstr_to_str("Fn((");
        TypeArray const params = type->as.fn_ptr_t.param_types;
        for (size_t i = 0; i < params.size; i++) {
            char const *fmt = i ? "%.*s, %.*s" : "%.*s%.*s";
            string const ts = _typeName(allocator, params[i]);
            s = string_format(allocator, fmt, s.size, s.data, ts.size, ts.data);
        }
        string const rts = _typeName(allocator, type->as.fn_ptr_t.ret_type);
        s = string_format(allocator, "%.*s), %.*s)", s.size, s.data, rts.size, rts.data);
        return s;
    }
    case Type_FnPtr: {
        string s = cstr_to_str("FnPtr((");
        TypeArray const params = type->as.fn_ptr_t.param_types;
        for (size_t i = 0; i < params.size; i++) {
            char const *fmt = i ? "%.*s, %.*s" : "%.*s%.*s";
            string const ts = _typeName(allocator, params[i]);
            s = string_format(allocator, fmt, s.size, s.data, ts.size, ts.data);
        }
        string const rts = _typeName(allocator, type->as.fn_ptr_t.ret_type);
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

type_t type_get_any() {
    _FpBase fp;
    std::memset(&fp, 0, sizeof(fp));
    fp.id = Type_Any;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = sizeof(value_t);
        res.type->alignment = alignof(value_t);
    }
    return res.type;
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
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = elem_count * elem_type->size;
        res.type->alignment = elem_type->alignment;
        res.type->as.arr_t.elem_type = elem_type;
        res.type->as.arr_t.elem_count = elem_count;
    }
    return res.type;
}

type_t type_get_array_ptr(type_t elem_type) {
    struct {
        _FpBase base;
        typeid_t elem_type;
    } fp;
    memset(&fp, 0, sizeof(fp));
    fp.base.id = Type_ArrayPtr;
    fp.elem_type = elem_type->id;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = 16;
        res.type->alignment = 8;
        res.type->as.arr_t.elem_type = elem_type;
    }
    return res.type;
}

type_t type_get_bool() {
    _FpBase fp;
    std::memset(&fp, 0, sizeof(fp));
    fp.id = Type_Bool;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = sizeof(bool);
        res.type->alignment = alignof(bool);
    }
    return res.type;
}

type_t type_get_fn(
    Allocator *tmp_allocator,
    type_t ret_type,
    TypeArray param_types,
    size_t decl_id,
    FuncPtr body,
    void *closure,
    bool args_passthrough) {
    struct Fp {
        _FpBase base;
        size_t decl_id;
        typeid_t ret_type;
        size_t param_count;
    };
    size_t const fp_size = arrayWithHeaderSize<Fp, typeid_t>(param_types.size);
    Fp *fp = (Fp *)tmp_allocator->alloc_aligned(fp_size, alignof(Fp));
    auto fp_params = arrayWithHeaderData<Fp, typeid_t>(fp);
    std::memset(fp, 0, fp_size);
    fp->base.id = Type_Fn;
    fp->decl_id = decl_id;
    fp->ret_type = ret_type->id;
    fp->param_count = param_types.size;
    for (size_t i = 0; i < param_types.size; i++) {
        fp_params[i] = param_types[i]->id;
    }
    _TypeQueryRes res = _getType({(uint8_t *)fp, fp_size});
    if (res.inserted) {
        type_t *params_ar_data = s_typearena.alloc<type_t>(param_types.size);
        std::memcpy(params_ar_data, param_types.data, param_types.size * sizeof(void *));

        res.type->size = 0;
        res.type->alignment = 1;
        res.type->as.fn_t.ret_type = ret_type;
        res.type->as.fn_t.param_types = {params_ar_data, param_types.size};
        res.type->as.fn_t.body = body;
        res.type->as.fn_t.closure = closure;
        res.type->as.fn_t.args_passthrough = args_passthrough;
    }
    return res.type;
}

type_t type_get_fn_ptr(Allocator *tmp_allocator, type_t ret_type, TypeArray param_types) {
    struct Fp {
        _FpBase base;
        typeid_t ret_type;
        size_t param_count;
    };
    size_t const fp_size = arrayWithHeaderSize<Fp, typeid_t>(param_types.size);
    Fp *fp = (Fp *)tmp_allocator->alloc_aligned(fp_size, alignof(Fp));
    auto fp_params = arrayWithHeaderData<Fp, typeid_t>(fp);
    std::memset(fp, 0, fp_size);
    fp->base.id = Type_FnPtr;
    fp->ret_type = ret_type->id;
    fp->param_count = param_types.size;
    for (size_t i = 0; i < param_types.size; i++) {
        fp_params[i] = param_types[i]->id;
    }
    _TypeQueryRes res = _getType({(uint8_t *)fp, fp_size});
    if (res.inserted) {
        type_t *params_ar_data = s_typearena.alloc<type_t>(param_types.size);
        std::memcpy(params_ar_data, param_types.data, param_types.size * sizeof(void *));

        res.type->size = 8;
        res.type->alignment = 8;
        res.type->as.fn_ptr_t.ret_type = ret_type;
        res.type->as.fn_ptr_t.param_types = {params_ar_data, param_types.size};
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
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        size_t const size = value_type & NUM_TYPE_SIZE_MASK;
        res.type->size = size;
        res.type->alignment = size;
        res.type->as.num_t.value_type = value_type;
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
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = sizeof(void *);
        res.type->alignment = alignof(void *);
        res.type->as.ptr_t.target_type = target_type;
    }
    return res.type;
}

type_t type_get_string() {
    _FpBase fp;
    std::memset(&fp, 0, sizeof(fp));
    fp.id = Type_String;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = sizeof(string);
        res.type->alignment = alignof(string);
    }
    return res.type;
}

type_t type_get_struct(Allocator *tmp_allocator, NameTypeArray fields, size_t decl_id) {
    struct Fp {
        _FpBase base;
        size_t decl_id;
        size_t field_count;
    };
    size_t const fp_size = arrayWithHeaderSize<Fp, NameTypeid>(fields.size);
    Fp *fp = (Fp *)tmp_allocator->alloc_aligned(fp_size, alignof(Fp));
    auto fp_fields = arrayWithHeaderData<Fp, NameTypeid>(fp);
    std::memset(fp, 0, fp_size);
    fp->base.id = Type_Struct;
    fp->decl_id = decl_id;
    fp->field_count = fields.size;
    for (size_t i = 0; i < fields.size; i++) {
        fp_fields[i].name = fields[i].name;
        fp_fields[i].id = fields[i].type->id;
    }
    _TypeQueryRes res = _getType({(uint8_t *)fp, fp_size});
    if (res.inserted) {
        _TupleLayout layout =
            _calcTupleLayout({&fields.data->type, fields.size}, sizeof(NameType) / sizeof(void *));

        Id *field_ar_data = s_typearena.alloc<Id>(fields.size);

        for (size_t i = 0; i < fields.size; i++) {
            field_ar_data[i] = fields[i].name;
        }

        res.type->size = layout.size;
        res.type->alignment = layout.alignment;
        res.type->as.struct_t.types = layout.info_ar;
        res.type->as.struct_t.fields = {field_ar_data, fields.size};
    }
    return res.type;
}

type_t type_get_symbol() {
    _FpBase fp;
    std::memset(&fp, 0, sizeof(fp));
    fp.id = Type_Symbol;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = sizeof(Id);
        res.type->alignment = alignof(Id);
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
        fp_types[i] = types[i]->id;
    }
    _TypeQueryRes res = _getType({(uint8_t *)fp, fp_size});
    if (res.inserted) {
        _TupleLayout layout = _calcTupleLayout({types.data, types.size}, 1);

        res.type->size = layout.size;
        res.type->alignment = layout.alignment;
        res.type->as.tuple_t.types = layout.info_ar;
    }
    return res.type;
}

type_t type_get_typeref() {
    _FpBase fp;
    std::memset(&fp, 0, sizeof(fp));
    fp.id = Type_Typeref;
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
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
    _TypeQueryRes res = _getType({(uint8_t *)&fp, sizeof(fp)});
    if (res.inserted) {
        res.type->size = 0;
        res.type->alignment = 1;
    }
    return res.type;
}

string type_name(Allocator *allocator, type_t type) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    size_t const frame = tmp_arena._seq.size;
    string tmp_str = _typeName(&tmp_arena, type);
    tmp_arena._seq.pop(tmp_arena._seq.size - frame);
    char *data = allocator->alloc<char>(tmp_str.size);
    std::memcpy(data, tmp_str.data, tmp_str.size);

    return string{data, tmp_str.size};
}

value_t val_undefined() {
    return value_t{nullptr, nullptr};
}

void *val_data(value_t val) {
    return val.data;
}

type_t val_typeof(value_t val) {
    return val.type;
}

size_t val_sizeof(value_t val) {
    return val.type->size;
}

size_t val_alignof(value_t val) {
    return val.type->alignment;
}

value_t val_reinterpret_cast(type_t type, value_t val) {
    return value_t{type, val.data};
}
