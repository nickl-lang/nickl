#include "types.h"

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

#define PUSH_VAL(fp, T, val)                        \
    do {                                            \
        T _val = (val);                             \
        nkda_appendMany((fp), &_val, sizeof(_val)); \
    } while (0)

typedef struct {
    NklType *type;
    bool inserted;
} TypeSearchResult;

static TypeSearchResult getTypeByFingerprint(ByteArray fp) {
    NK_PROF_FUNC_BEGIN();
    NK_LOG_TRC("%s", __func__);

    TypeSearchResult res = {0};

    nk_mutex_lock(g_mtx);

    Type_kv *found = TypeTree_find(&g_type_tree, fp);
    if (found) {
        res.type = found->val;
    } else {
        res.type = nk_arena_alloc(&g_type_arena, sizeof(NklType));
        res.inserted = true;

        *res.type = (NklType){0};
        res.type->id = g_next_id++;

        ByteArray fp_copy;
        nk_slice_copy(nk_arena_getAllocator(&g_type_arena), &fp_copy, fp);

        TypeTree_insert(&g_type_tree, (Type_kv){fp_copy, res.type});
    }

    nk_mutex_unlock(g_mtx);

    NK_PROF_FUNC_END();
    return res;
}

void nkl_types_init() {
    g_mtx = nk_mutex_alloc();
}

void nkl_types_free() {
    nk_mutex_free(g_mtx);
}

nkltype_t nkl_get_any() {
}

nkltype_t nkl_get_array(nkltype_t elem_type, size_t elem_count) {
}

nkltype_t nkl_get_enum(NklFieldArray fields) {
}

nkltype_t nkl_get_numeric(NkIrNumericValueType value_type) {
}

nkltype_t nkl_get_proc(NklProcInfo info) {
}

nkltype_t nkl_get_ptr(nkltype_t target_type, bool is_const) {
}

nkltype_t nkl_get_slice(nkltype_t target_type, bool is_const) {
}

nkltype_t nkl_get_struct(NklFieldArray fields) {
}

nkltype_t nkl_get_struct_packed(NklFieldArray fields) {
}

nkltype_t nkl_get_tuple(NklTypeArray types) {
}

nkltype_t nkl_get_tuple_packed(NklTypeArray types) {
}

nkltype_t nkl_get_typeref() {
}

nkltype_t nkl_get_union(NklFieldArray fields) {
}

nkltype_t nkl_get_void() {
    NklTypeClass const tclass = NklType_Tuple;

    NkArenaFrame frame = nk_arena_grab(&g_tmp_arena);

    ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&g_tmp_arena))};
    // TODO PUSH_VAL(&fp, u8, NklTypeSubset);
    PUSH_VAL(&fp, u8, tclass);
    PUSH_VAL(&fp, u64, 0);

    TypeSearchResult res = getTypeByFingerprint((ByteArray){NK_SLICE_INIT(fp)});

    if (res.inserted) {
        // TODO res.type->ir_type =
        res.type->tclass = tclass;
    }

    nk_arena_popFrame(&g_tmp_arena, frame);
    return res.type;
}

void nkl_type_inspect(nkltype_t type, NkStream out) {
}
