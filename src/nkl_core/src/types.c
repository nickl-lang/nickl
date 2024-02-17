#include "nkl/core/types.h"

#include "nkb/common.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/stream.h"

NK_LOG_USE_SCOPE(types);

typedef NkDynArray(u8) ByteDynArray;

typedef enum {
    TypeSubset_Ir,
    TypeSubset_Nkl,
} TypeSubset;

static ByteArray const *Type_kv_GetKey(Type_kv const *item) {
    return &item->key;
}

static u64 ByteArray_hash(ByteArray const key) {
    return nk_hashArray(key.data, key.data + key.size);
}

static bool ByteArray_equal(ByteArray const lhs, ByteArray const rhs) {
    return lhs.size == rhs.size && memcmp(lhs.data, lhs.data, lhs.size) == 0;
}

NK_HASH_TREE_IMPL(TypeTree, Type_kv, ByteArray, Type_kv_GetKey, ByteArray_hash, ByteArray_equal);

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

static TypeSearchResult getTypeByFingerprint(NklState *nkl, ByteArray fp, NklType *backing) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    TypeSearchResult res = {0};

    nk_mutex_lock(nkl->types.mtx);

    Type_kv *found = TypeTree_find(&nkl->types.type_tree, fp);
    if (found) {
        res.type = found->val;
    } else {
        if (backing) {
            res.type = backing;
        } else {
            res.type = nk_arena_alloc(&nkl->types.type_arena, sizeof(NklType));
            *res.type = (NklType){0};
        }
        res.inserted = true;
        res.id = nkl->types.next_id++;

        ByteArray fp_copy;
        nk_slice_copy(nk_arena_getAllocator(&nkl->types.type_arena), &fp_copy, fp);

        TypeTree_insert(&nkl->types.type_tree, (Type_kv){fp_copy, res.type});
    }

    nk_mutex_unlock(nkl->types.mtx);

    NK_PROF_FUNC_END();
    return res;
}

void nkl_types_init(NklState *nkl) {
    nkl->types = (NklTypeStorage){
        .type_arena = {0},
        .type_tree = {NULL, nk_arena_getAllocator(&nkl->types.type_arena)},
        .mtx = nk_mutex_alloc(),
        .next_id = 1,

        .tmp_arena = {0},
    };
}

void nkl_types_free(NklState *nkl) {
    nk_mutex_free(nkl->types.mtx);

    nk_arena_free(&nkl->types.type_arena);
    nk_arena_free(&nkl->types.tmp_arena);
}

static void get_ir_aggregate(NklState *nkl, NklType *backing, NkIrAggregateLayout const layout) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Aggregate;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Ir);
    PUSH_VAL(&fp, u8, kind);
    PUSH_VAL(&fp, usize, layout.info_ar.size);
    for (usize i = 0; i < layout.info_ar.size; i++) {
        PUSH_VAL(&fp, u32, layout.info_ar.data[i].type->id);
        PUSH_VAL(&fp, u32, layout.info_ar.data[i].count);
    }

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, backing);

    if (res.inserted) {
        backing->ir_type = (NkIrType){
            .as = {{{0}}},
            .size = layout.size,
            .flags = 0,
            .align = layout.align,
            .kind = kind,
            .id = res.id,
        };
        nk_slice_copy(nk_arena_getAllocator(&nkl->types.type_arena), &backing->ir_type.as.aggr.elems, layout.info_ar);
    } else {
        backing->ir_type = res.type->ir_type;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
}

static void get_ir_numeric(NklState *nkl, NklType *backing, NkIrNumericValueType value_type) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Numeric;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Ir);
    PUSH_VAL(&fp, u8, kind);
    PUSH_VAL(&fp, u8, value_type);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, backing);

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

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
}

static void get_ir_ptr(NklState *nkl, NklType *backing, nktype_t target_type) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Pointer;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Ir);
    PUSH_VAL(&fp, u8, kind);
    PUSH_VAL(&fp, u32, target_type->id);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, backing);

    if (res.inserted) {
        backing->ir_type = (NkIrType){
            .as = {.ptr = {.target_type = target_type}},
            .size = sizeof(void *), // TODO nkl->ptr_size,
            .flags = 0,
            .align = sizeof(void *), // TODO nkl->ptr_size,
            .kind = kind,
            .id = res.id,
        };
    } else {
        backing->ir_type = res.type->ir_type;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
}

static void get_ir_proc(NklState *nkl, NklType *backing, NklProcInfo info) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Procedure;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Ir);
    PUSH_VAL(&fp, u8, kind);
    PUSH_VAL(&fp, usize, info.param_types.size);
    for (usize i = 0; i < info.param_types.size; i++) {
        PUSH_VAL(&fp, u32, info.param_types.data[i]->id);
    }
    PUSH_VAL(&fp, u32, info.ret_t->id);
    PUSH_VAL(&fp, u8, info.call_conv);
    PUSH_VAL(&fp, u8, info.flags);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, backing);

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
            .size = sizeof(void *), // TODO nkl->ptr_size,
            .flags = 0,
            .align = sizeof(void *), // TODO nkl->ptr_size,
            .kind = kind,
            .id = res.id,
        };
        // TODO Improve ProcInfo copying
        nk_slice_copy(
            nk_arena_getAllocator(&nkl->types.type_arena), &backing->ir_type.as.proc.info.args_t, info.param_types);
    } else {
        backing->ir_type = res.type->ir_type;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
}

nkltype_t nkl_get_any(NklState *nkl) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Any;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        NklField fields[] = {
            {
                nk_cs2atom("data"),
                nkl_get_ptr(nkl, nkl_get_void(nkl), false),
            },
            {
                nk_cs2atom("type"),
                nkl_get_typeref(nkl),
            },
        };
        nkltype_t const underlying_type = nkl_get_struct(nkl, (NklFieldArray){fields, NK_ARRAY_COUNT(fields)});

        res.type->ir_type = underlying_type->ir_type;
        res.type->as = underlying_type->as;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = underlying_type;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_array(NklState *nkl, nkltype_t elem_type, usize elem_count) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Array;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, u32, elem_type->id);
    PUSH_VAL(&fp, usize, elem_count);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        nktype_t elem_types[] = {&elem_type->ir_type};
        usize elem_counts[] = {elem_count};

        NkIrAggregateLayout layout =
            nkir_calcAggregateLayout(nk_arena_getAllocator(&nkl->types.tmp_arena), elem_types, elem_counts, 1, 1, 1);
        get_ir_aggregate(nkl, res.type, layout);

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_enum(NklState *nkl, NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Enum;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, fields.size);
    for (usize i = 0; i < fields.size; i++) {
        PUSH_VAL(&fp, u32, fields.data[i].name);
        PUSH_VAL(&fp, u32, fields.data[i].type->id);
    }

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        NklField enum_fields[] = {
            {
                .name = nk_cs2atom("data"),
                .type = nkl_get_union(nkl, fields),
            },
            {
                .name = nk_cs2atom("tag"), .type = nkl_get_numeric(nkl, Uint64), // TODO Hardcoded 64bit enum tag
            },
        };
        nkltype_t underlying_type = nkl_get_struct(nkl, (NklFieldArray){enum_fields, NK_ARRAY_COUNT(enum_fields)});

        res.type->ir_type = underlying_type->ir_type;
        res.type->as = underlying_type->as;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = underlying_type;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_numeric(NklState *nkl, NkIrNumericValueType value_type) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Numeric;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, u8, value_type);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        get_ir_numeric(nkl, res.type, value_type);

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_proc(NklState *nkl, NklProcInfo info) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Procedure;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, info.param_types.size);
    for (usize i = 0; i < info.param_types.size; i++) {
        PUSH_VAL(&fp, u32, info.param_types.data[i]->id);
    }
    PUSH_VAL(&fp, u32, info.ret_t->id);
    PUSH_VAL(&fp, u8, info.call_conv);
    PUSH_VAL(&fp, u8, info.flags);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        get_ir_proc(nkl, res.type, info);

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_ptr(NklState *nkl, nkltype_t target_type, bool is_const) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Pointer;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, u32, target_type->id);
    PUSH_VAL(&fp, u8, is_const);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        get_ir_ptr(nkl, res.type, &target_type->ir_type);

        res.type->as.ptr.is_const = is_const;

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_slice(NklState *nkl, nkltype_t target_type, bool is_const) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Slice;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, u32, target_type->id);
    PUSH_VAL(&fp, u8, is_const);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        NklField fields[] = {
            {
                nk_cs2atom("data"),
                nkl_get_ptr(nkl, target_type, is_const),
            },
            {
                nk_cs2atom("size"), nkl_get_numeric(nkl, Uint64), // TODO Should use g_ptr_size
            },
        };
        nkltype_t const underlying_type = nkl_get_struct(nkl, (NklFieldArray){fields, NK_ARRAY_COUNT(fields)});

        res.type->ir_type = underlying_type->ir_type;
        res.type->as = underlying_type->as;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = underlying_type;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_struct(NklState *nkl, NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Struct;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, fields.size);
    for (usize i = 0; i < fields.size; i++) {
        PUSH_VAL(&fp, u32, fields.data[i].name);
        PUSH_VAL(&fp, u32, fields.data[i].type->id);
    }

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        nkltype_t const underlying_type =
            nkl_get_tupleEx(nkl, &fields.data->type, fields.size, sizeof(*fields.data) / sizeof(void *));

        nk_slice_copy(nk_arena_getAllocator(&nkl->types.type_arena), &res.type->as.strct.fields, fields);

        res.type->ir_type = underlying_type->ir_type;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = underlying_type;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_struct_packed(NklState *nkl, NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    (void)nkl;
    (void)fields;
    nk_assert(!"nkl_get_struct_packed is not implemented");
    return NULL;
}

nkltype_t nkl_get_tuple(NklState *nkl, NklTypeArray types) {
    return nkl_get_tupleEx(nkl, types.data, types.size, 1);
}

nkltype_t nkl_get_tupleEx(NklState *nkl, nkltype_t const *types, usize count, usize stride) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Tuple;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, count);
    for (usize i = 0; i < count; i++) {
        PUSH_VAL(&fp, u32, types[i * stride]->id);
    }

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        NkIrAggregateLayout layout = nkir_calcAggregateLayout(
            nk_arena_getAllocator(&nkl->types.tmp_arena), (nktype_t *)types, NULL, count, stride, 1);
        get_ir_aggregate(nkl, res.type, layout);

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_tuple_packed(NklState *nkl, NklTypeArray types) {
    NK_LOG_TRC("%s", __func__);

    (void)nkl;
    (void)types;
    nk_assert(!"nkl_get_tuple_packed is not implemented");
    return NULL;
}

nkltype_t nkl_get_typeref(NklState *nkl) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Typeref;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        nkltype_t const underlying_type = nkl_get_ptr(nkl, nkl_get_void(nkl), true);

        res.type->ir_type = underlying_type->ir_type;
        res.type->as = underlying_type->as;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = underlying_type;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_union(NklState *nkl, NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Union;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, fields.size);
    for (usize i = 0; i < fields.size; i++) {
        PUSH_VAL(&fp, u32, fields.data[i].name);
        PUSH_VAL(&fp, u32, fields.data[i].type->id);
    }

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        nkltype_t largest_type = nkl_get_void(nkl);
        u8 max_align = 1;
        for (usize i = 0; i < fields.size; i++) {
            nkltype_t const type = fields.data[i].type;
            if (type->ir_type.size > largest_type->ir_type.size) {
                largest_type = type;
            }
            max_align = nk_maxu(max_align, type->ir_type.align);
        }

        nk_slice_copy(nk_arena_getAllocator(&nkl->types.type_arena), &res.type->as.strct.fields, fields);

        // TODO Use a special ir_type for union? bytes or smth?
        res.type->ir_type = largest_type->ir_type;
        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_void(NklState *nkl) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Tuple;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&nkl->types.tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, 0);

    TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        get_ir_aggregate(nkl, res.type, (NkIrAggregateLayout){{0}, 0, 1});

        res.type->tclass = tclass;
        res.type->id = res.id;
    }

    nk_arena_popFrame(&nkl->types.tmp_arena, frame);
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
