#include "nkl/core/types.h"

#include "nkb/common.h"
#include "ntk/arena.h"
#include "ntk/dyn_array.h"
#include "ntk/hash_tree.h"
#include "ntk/log.h"
#include "ntk/os/thread.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/stream.h"

NK_LOG_USE_SCOPE(types);

typedef NkDynArray(u8) ByteDynArray;
typedef NkSlice(u8) ByteArray;

typedef enum {
    TypeSubset_Ir,
    TypeSubset_Nkl,
} TypeSubset;

typedef struct {
    ByteArray key;
    NklType *val;
} Type_kv;

static ByteArray const *Type_kv_GetKey(Type_kv const *item) {
    return &item->key;
}

static u64 ByteArray_hash(ByteArray const key) {
    return nk_hashArray(key.data, key.data + key.size);
}

static bool ByteArray_equal(ByteArray const lhs, ByteArray const rhs) {
    return lhs.size == rhs.size && memcmp(lhs.data, lhs.data, lhs.size) == 0;
}

// Use hash map for types
NK_HASH_TREE_DEFINE(TypeTree, Type_kv, ByteArray, Type_kv_GetKey, ByteArray_hash, ByteArray_equal);

// TODO Do not use static storage
static NkArena g_type_arena;
static TypeTree g_type_tree;
static NkOsHandle g_mtx;
static u32 g_next_id = 1;

// TODO Use scratch arenas
static NkArena g_tmp_arena;

static u8 g_ptr_size;

#define PUSH_VAL(fp, T, val)                        \
    do {                                            \
        T _val = (val);                             \
        nkda_appendMany((fp), &_val, sizeof(_val)); \
    } while (0)

typedef struct {
    NklType *type;
    bool inserted;
    u32 id;
} TypeSearchResult;

static TypeSearchResult getTypeByFingerprint(ByteArray fp, NklType *backing) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    TypeSearchResult res = {0};

    nk_mutex_lock(g_mtx);

    Type_kv *found = TypeTree_find(&g_type_tree, fp);
    if (found) {
        res.type = found->val;
    } else {
        if (backing) {
            res.type = backing;
        } else {
            res.type = nk_arena_alloc(&g_type_arena, sizeof(NklType));
            *res.type = (NklType){0};
        }
        res.inserted = true;
        res.id = g_next_id++;

        ByteArray fp_copy;
        nk_slice_copy(nk_arena_getAllocator(&g_type_arena), &fp_copy, fp);

        TypeTree_insert(&g_type_tree, (Type_kv){fp_copy, res.type});
    }

    nk_mutex_unlock(g_mtx);

    NK_PROF_FUNC_END();
    return res;
}

void nkl_types_init(u8 ptr_size) {
    g_type_tree.alloc = nk_arena_getAllocator(&g_type_arena);
    g_mtx = nk_mutex_alloc();
    g_ptr_size = ptr_size;
}

void nkl_types_free() {
    nk_mutex_free(g_mtx);
}

static void get_ir_aggregate(NklType *backing, NkIrAggregateLayout const layout) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Aggregate;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Ir);
    PUSH_VAL(&fp, u8, kind);
    PUSH_VAL(&fp, usize, layout.info_ar.size);
    for (usize i = 0; i < layout.info_ar.size; i++) {
        PUSH_VAL(&fp, u32, layout.info_ar.data[i].type->id);
        PUSH_VAL(&fp, u32, layout.info_ar.data[i].count);
    }

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, backing);

    if (res.inserted) {
        backing->ir_type = (NkIrType){
            .as = {{{0}}},
            .size = layout.size,
            .flags = 0,
            .align = layout.align,
            .kind = kind,
            .id = res.id,
        };
        nk_slice_copy(nk_arena_getAllocator(&g_type_arena), &backing->ir_type.as.aggr.elems, layout.info_ar);
    } else {
        backing->ir_type = res.type->ir_type;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
}

static void get_ir_numeric(NklType *backing, NkIrNumericValueType value_type) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Numeric;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Ir);
    PUSH_VAL(&fp, u8, kind);
    PUSH_VAL(&fp, u8, value_type);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, backing);

    if (res.inserted) {
        backing->ir_type = (NkIrType){
            .as = {.num = {.value_type = value_type}},
            .size = NKIR_NUMERIC_TYPE_SIZE(value_type),
            .flags = 0,
            .align = NKIR_NUMERIC_TYPE_SIZE(value_type),
            .kind = kind,
            .id = res.id,
        };
    } else {
        backing->ir_type = res.type->ir_type;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
}

static void get_ir_ptr(NklType *backing, nktype_t target_type) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Pointer;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Ir);
    PUSH_VAL(&fp, u8, kind);
    PUSH_VAL(&fp, u32, target_type->id);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, backing);

    if (res.inserted) {
        backing->ir_type = (NkIrType){
            .as = {.ptr = {.target_type = target_type}},
            .size = g_ptr_size,
            .flags = 0,
            .align = g_ptr_size,
            .kind = kind,
            .id = res.id,
        };
    } else {
        backing->ir_type = res.type->ir_type;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
}

static void get_ir_proc(NklType *backing, NklProcInfo info) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Procedure;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Ir);
    PUSH_VAL(&fp, u8, kind);
    PUSH_VAL(&fp, usize, info.param_types.size);
    for (usize i = 0; i < info.param_types.size; i++) {
        PUSH_VAL(&fp, u32, info.param_types.data[i]->id);
    }
    PUSH_VAL(&fp, u32, info.ret_t->id);
    PUSH_VAL(&fp, u8, info.call_conv);
    PUSH_VAL(&fp, u8, info.flags);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, backing);

    if (res.inserted) {
        backing->ir_type = (NkIrType){
            .as =
                {.proc =
                     {.info =
                          {
                              .ret_t = (nktype_t)info.ret_t,
                              .call_conv = info.call_conv,
                              .flags = info.flags,
                          }}},
            .size = g_ptr_size,
            .flags = 0,
            .align = g_ptr_size,
            .kind = kind,
            .id = res.id,
        };
        // TODO Improve ProcInfo copying
        nk_slice_copy(
            nk_arena_getAllocator(&g_type_arena),
            (NklTypeArray *)&backing->ir_type.as.proc.info.args_t,
            info.param_types);
    } else {
        backing->ir_type = res.type->ir_type;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
}

nkltype_t nkl_get_any() {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Any;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        NklField fields[] = {
            {
                nk_cs2atom("data"),
                nkl_get_ptr(nkl_get_void(), false),
            },
            {
                nk_cs2atom("type"),
                nkl_get_typeref(),
            },
        };
        nkltype_t const underlying_type = nkl_get_struct((NklFieldArray){fields, NK_ARRAY_COUNT(fields)});

        res.type->ir_type = underlying_type->ir_type;
        res.type->as = underlying_type->as;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = underlying_type;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_array(nkltype_t elem_type, usize elem_count) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Array;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, u32, elem_type->id);
    PUSH_VAL(&fp, usize, elem_count);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        nktype_t elem_types[] = {&elem_type->ir_type};
        usize elem_counts[] = {elem_count};

        NkIrAggregateLayout layout =
            nkir_calcAggregateLayout(nk_arena_getAllocator(&g_tmp_arena), elem_types, elem_counts, 1, 1, 1);
        get_ir_aggregate(res.type, layout);

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_enum(NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Enum;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, fields.size);
    for (usize i = 0; i < fields.size; i++) {
        PUSH_VAL(&fp, u32, fields.data[i].name);
        PUSH_VAL(&fp, u32, fields.data[i].type->id);
    }

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        NklField enum_fields[] = {
            {
                .name = nk_cs2atom("data"),
                .type = nkl_get_union(fields),
            },
            {
                .name = nk_cs2atom("tag"),
                .type = nkl_get_numeric(Uint64), // TODO Hardcoded 64bit enum tag
            },
        };
        nkltype_t underlying_type = nkl_get_struct((NklFieldArray){enum_fields, NK_ARRAY_COUNT(enum_fields)});

        res.type->ir_type = underlying_type->ir_type;
        res.type->as = underlying_type->as;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = underlying_type;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_numeric(NkIrNumericValueType value_type) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Numeric;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, u8, value_type);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        get_ir_numeric(res.type, value_type);

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_proc(NklProcInfo info) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Procedure;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, info.param_types.size);
    for (usize i = 0; i < info.param_types.size; i++) {
        PUSH_VAL(&fp, u32, info.param_types.data[i]->id);
    }
    PUSH_VAL(&fp, u32, info.ret_t->id);
    PUSH_VAL(&fp, u8, info.call_conv);
    PUSH_VAL(&fp, u8, info.flags);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        get_ir_proc(res.type, info);

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_ptr(nkltype_t target_type, bool is_const) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Pointer;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, u32, target_type->id);
    PUSH_VAL(&fp, u8, is_const);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        get_ir_ptr(res.type, &target_type->ir_type);

        res.type->as.ptr.is_const = is_const;

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_slice(nkltype_t target_type, bool is_const) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Slice;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, u32, target_type->id);
    PUSH_VAL(&fp, u8, is_const);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        NklField fields[] = {
            {
                nk_cs2atom("data"),
                nkl_get_ptr(target_type, is_const),
            },
            {
                nk_cs2atom("size"),
                nkl_get_numeric(Uint64), // TODO Should use g_ptr_size
            },
        };
        nkltype_t const underlying_type = nkl_get_struct((NklFieldArray){fields, NK_ARRAY_COUNT(fields)});

        res.type->ir_type = underlying_type->ir_type;
        res.type->as = underlying_type->as;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = underlying_type;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_struct(NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Struct;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, fields.size);
    for (usize i = 0; i < fields.size; i++) {
        PUSH_VAL(&fp, u32, fields.data[i].name);
        PUSH_VAL(&fp, u32, fields.data[i].type->id);
    }

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        nkltype_t const underlying_type =
            nkl_get_tupleEx(&fields.data->type, fields.size, sizeof(*fields.data) / sizeof(void *));

        nk_slice_copy(nk_arena_getAllocator(&g_type_arena), &res.type->as.strct.fields, fields);

        res.type->ir_type = underlying_type->ir_type;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = underlying_type;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_struct_packed(NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    (void)fields;
    nk_assert(!"nkl_get_struct_packed is not implemented");
    return NULL;
}

nkltype_t nkl_get_tuple(NklTypeArray types) {
    return nkl_get_tupleEx(types.data, types.size, 1);
}

nkltype_t nkl_get_tupleEx(nkltype_t const *types, usize count, usize stride) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Tuple;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, count);
    for (usize i = 0; i < count; i++) {
        PUSH_VAL(&fp, u32, types[i * stride]->id);
    }

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        NkIrAggregateLayout layout =
            nkir_calcAggregateLayout(nk_arena_getAllocator(&g_tmp_arena), (nktype_t *)types, NULL, count, stride, 1);
        get_ir_aggregate(res.type, layout);

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_tuple_packed(NklTypeArray types) {
    NK_LOG_TRC("%s", __func__);

    (void)types;
    nk_assert(!"nkl_get_tuple_packed is not implemented");
    return NULL;
}

nkltype_t nkl_get_typeref() {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Typeref;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        nkltype_t const underlying_type = nkl_get_ptr(nkl_get_void(), true);

        res.type->ir_type = underlying_type->ir_type;
        res.type->as = underlying_type->as;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = underlying_type;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_union(NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Union;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, fields.size);
    for (usize i = 0; i < fields.size; i++) {
        PUSH_VAL(&fp, u32, fields.data[i].name);
        PUSH_VAL(&fp, u32, fields.data[i].type->id);
    }

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        nkltype_t largest_type = nkl_get_void();
        u8 max_align = 1;
        for (usize i = 0; i < fields.size; i++) {
            nkltype_t const type = fields.data[i].type;
            if (type->ir_type.size > largest_type->ir_type.size) {
                largest_type = type;
            }
            max_align = nk_maxu(max_align, type->ir_type.align);
        }

        nk_slice_copy(nk_arena_getAllocator(&g_type_arena), &res.type->as.strct.fields, fields);

        // TODO Use a special ir_type for union? bytes or smth?
        res.type->ir_type = largest_type->ir_type;
        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_void() {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Tuple;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, 0);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        get_ir_aggregate(res.type, (NkIrAggregateLayout){{0}, 0, 1});

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

void nkl_type_inspect(nkltype_t type, NkStream out) {
    NK_LOG_TRC("%s", __func__);

    switch (type->tclass) {
    case NklType_Any:
        nk_stream_printf(out, "any_t");
        break;
    case NklType_Array: {
        NkIrAggregateElemInfo info = type->ir_type.as.aggr.elems.data[0];
        nk_stream_printf(out, "[%zu]", info.count);
        nkl_type_inspect((nkltype_t)info.type, out);
        break;
    }
    case NklType_Enum: {
        nk_stream_printf(out, "enum { ");
        nkltype_t union_t = (nkltype_t)type->ir_type.as.aggr.elems.data[0].type;
        NklFieldArray fields = union_t->as.strct.fields;
        for (usize i = 0; i < fields.size; i++) {
            nk_stream_printf(out, "%s", nk_atom2cs(fields.data[i].name));
            nk_stream_printf(out, ": ");
            nkl_type_inspect((nkltype_t)fields.data[i].type, out);
            nk_stream_printf(out, ", ");
        }
        nk_stream_printf(out, "}");
        break;
    }
    case NklType_Numeric:
        switch (type->ir_type.as.num.value_type) {
        case Int8:
            nk_stream_printf(out, "i8");
            break;
        case Int16:
            nk_stream_printf(out, "i16");
            break;
        case Int32:
            nk_stream_printf(out, "i32");
            break;
        case Int64:
            nk_stream_printf(out, "i64");
            break;
        case Uint8:
            nk_stream_printf(out, "u8");
            break;
        case Uint16:
            nk_stream_printf(out, "u16");
            break;
        case Uint32:
            nk_stream_printf(out, "u32");
            break;
        case Uint64:
            nk_stream_printf(out, "u64");
            break;
        case Float32:
            nk_stream_printf(out, "f32");
            break;
        case Float64:
            nk_stream_printf(out, "f64");
            break;
        default:
            nk_assert(!"unreachable");
            break;
        }
        break;
    case NklType_Pointer: {
        nkltype_t target_type = (nkltype_t)type->ir_type.as.ptr.target_type;
        nk_stream_printf(out, "*");
        if (type->as.ptr.is_const) {
            nk_stream_printf(out, "const ");
        }
        nkl_type_inspect(target_type, out);
        break;
    }
    case NklType_Procedure: {
        NkIrProcInfo info = type->ir_type.as.proc.info;
        nk_stream_printf(out, "(");
        for (usize i = 0; i < info.args_t.size; i++) {
            if (i) {
                nk_stream_printf(out, ", ");
            }
            nkltype_t arg_t = (nkltype_t)info.args_t.data[i];
            nkl_type_inspect(arg_t, out);
        }
        nk_stream_printf(out, ") -> ");
        nkltype_t ret_t = (nkltype_t)info.ret_t;
        nkl_type_inspect(ret_t, out);
        break;
    }
    case NklType_Slice: {
        nkltype_t ptr_t = (nkltype_t)type->ir_type.as.aggr.elems.data[0].type;
        nk_stream_printf(out, "[]");
        if (ptr_t->as.ptr.is_const) {
            nk_stream_printf(out, "const");
        }
        nk_stream_printf(out, " ");
        nkl_type_inspect((nkltype_t)ptr_t->ir_type.as.ptr.target_type, out);
        break;
    }
    case NklType_Struct: {
        nk_stream_printf(out, "struct { ");
        NklFieldArray fields = type->as.strct.fields;
        for (usize i = 0; i < fields.size; i++) {
            nk_stream_printf(out, "%s", nk_atom2cs(fields.data[i].name));
            nk_stream_printf(out, ": ");
            nkl_type_inspect((nkltype_t)fields.data[i].type, out);
            nk_stream_printf(out, ", ");
        }
        nk_stream_printf(out, "}");
        break;
    }
    case NklType_StructPacked:
        nk_stream_printf(out, "<StructPacked inspect is not implemented>");
        break;
    case NklType_Tuple: {
        NkIrAggregateElemInfoArray elems = type->ir_type.as.aggr.elems;
        if (elems.size) {
            nk_stream_printf(out, "(");
            for (usize i = 0; i < elems.size; i++) {
                nkltype_t elem_t = (nkltype_t)elems.data[i].type;
                nkl_type_inspect(elem_t, out);
                nk_stream_printf(out, ", ");
            }
            nk_stream_printf(out, ")");
        } else {
            nk_stream_printf(out, "void");
        }
        break;
    }
    case NklType_TuplePacked:
        nk_stream_printf(out, "<TuplePacked inspect is not implemented>");
        break;
    case NklType_Typeref:
        nk_stream_printf(out, "type_t");
        break;
    case NklType_Union: {
        nk_stream_printf(out, "union { ");
        NklFieldArray fields = type->as.strct.fields;
        for (usize i = 0; i < fields.size; i++) {
            nk_stream_printf(out, "%s", nk_atom2cs(fields.data[i].name));
            nk_stream_printf(out, ": ");
            nkl_type_inspect((nkltype_t)fields.data[i].type, out);
            nk_stream_printf(out, ", ");
        }
        nk_stream_printf(out, "}");
        break;
    }

    default:
        nk_assert(!"unreachable");
    }
}
