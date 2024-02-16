#include "nkl/core/types.h"

#include "nkb/common.h"
#include "ntk/arena.h"
#include "ntk/dyn_array.h"
#include "ntk/hash_tree.h"
#include "ntk/log.h"
#include "ntk/os/thread.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"

NK_LOG_USE_SCOPE(types);

typedef NkDynArray(u8) ByteDynArray;
typedef NkSlice(u8) ByteArray;

typedef enum {
    TypeSubset_NkIr,
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
    PUSH_VAL(&fp, u8, TypeSubset_NkIr);
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
    PUSH_VAL(&fp, u8, TypeSubset_NkIr);
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
    PUSH_VAL(&fp, u8, TypeSubset_NkIr);
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

static void get_ir_void(NklType *backing) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Aggregate;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_NkIr);
    PUSH_VAL(&fp, u8, kind);
    PUSH_VAL(&fp, u64, 0);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, backing);

    if (res.inserted) {
        backing->ir_type = (NkIrType){
            .as = {{{0}}},
            .size = 0,
            .flags = 0,
            .align = 1,
            .kind = kind,
            .id = res.id,
        };
    } else {
        backing->ir_type = res.type->ir_type;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
}

nkltype_t nkl_get_any() {
    NK_LOG_TRC("%s", __func__);
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
        res.type->underlying_type = NULL;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_enum(NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);
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
        res.type->underlying_type = NULL;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_proc(NklProcInfo info) {
    NK_LOG_TRC("%s", __func__);
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
        res.type->underlying_type = NULL;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_slice(nkltype_t target_type, bool is_const) {
    NK_LOG_TRC("%s", __func__);
}

nkltype_t nkl_get_struct(NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);
}

nkltype_t nkl_get_struct_packed(NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);
}

nkltype_t nkl_get_tuple(NklTypeArray types) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Tuple;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, usize, types.size);
    for (usize i = 0; i < types.size; i++) {
        PUSH_VAL(&fp, u32, types.data[i]->id);
    }

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        NkIrAggregateLayout layout = nkir_calcAggregateLayout(
            nk_arena_getAllocator(&g_tmp_arena), (nktype_t *)types.data, NULL, types.size, 1, 1);
        get_ir_aggregate(res.type, layout);

        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = NULL;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_tuple_packed(NklTypeArray types) {
    NK_LOG_TRC("%s", __func__);
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
        nkltype_t const void_ptr_t = nkl_get_ptr(nkl_get_void(), false);

        res.type->ir_type = void_ptr_t->ir_type;
        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = void_ptr_t;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

nkltype_t nkl_get_union(NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);
}

nkltype_t nkl_get_void() {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Tuple;

    // TODO Use scope macro
    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    PUSH_VAL(&fp, u8, TypeSubset_Nkl);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, u64, 0);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)}, NULL);

    if (res.inserted) {
        get_ir_void(res.type);

        res.type->tclass = tclass;
        res.type->id = res.id;
        res.type->underlying_type = NULL;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

void nkl_type_inspect(nkltype_t type, NkStream out) {
    NK_LOG_TRC("%s", __func__);
}
