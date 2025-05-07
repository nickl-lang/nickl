#include "nkl/core/types.h"

#include "nickl_impl.h"
#include "nkb/common.h"
#include "ntk/arena.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/stream.h"
#include "ntk/thread.h"

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

NK_HASH_TREE_IMPL(TypeMap, Type_kv, ByteArray, Type_kv_GetKey, ByteArray_hash, ByteArray_equal);

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

static TypeSearchResult getTypeByFingerprint(NklState nkl, ByteArray fp, NklType *backing) {
    NK_LOG_TRC("%s", __func__);

    TypeSearchResult res = {0};
    NK_PROF_FUNC() {
        nk_mutex_lock(nkl->types.mtx);

        Type_kv *found = TypeMap_find(&nkl->types.type_map, fp);
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

            TypeMap_insert(&nkl->types.type_map, (Type_kv){fp_copy, res.type});
        }

        nk_mutex_unlock(nkl->types.mtx);
    }
    return res;
}

void nkl_types_init(NklState nkl) {
    nkl->types = (NklTypeStorage){
        .type_arena = {0},
        .type_map = {NULL, nk_arena_getAllocator(&nkl->types.type_arena)},
        .mtx = nk_mutex_alloc(),
        .next_id = 1,

        .tmp_arena = {0},
    };
}

void nkl_types_free(NklState nkl) {
    nk_mutex_free(nkl->types.mtx);

    nk_arena_free(&nkl->types.type_arena);
    nk_arena_free(&nkl->types.tmp_arena);
}

static void get_ir_aggregate(NklState nkl, NklType *backing, NkIrAggregateLayout const layout) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Aggregate;

    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
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
            nk_slice_copy(
                nk_arena_getAllocator(&nkl->types.type_arena), &backing->ir_type.as.aggr.elems, layout.info_ar);
        } else {
            backing->ir_type = res.type->ir_type;
        }
    }
}

static void get_ir_numeric(NklState nkl, NklType *backing, NkIrNumericValueType value_type) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Numeric;

    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
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
    }
}

static void get_ir_ptr(NklState nkl, usize word_size, NklType *backing, nktype_t target_type) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Pointer;

    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Ir);
        PUSH_VAL(&fp, u8, kind);
        PUSH_VAL(&fp, u32, target_type->id);

        TypeSearchResult res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, backing);

        if (res.inserted) {
            backing->ir_type = (NkIrType){
                .as = {{{0}}},
                .size = word_size,
                .flags = 0,
                .align = word_size,
                .kind = kind,
                .id = res.id,
            };
        } else {
            backing->ir_type = res.type->ir_type;
        }
    }
}

static void get_ir_proc(NklState nkl, usize word_size, NklType *backing, NklProcInfo info) {
    NK_LOG_TRC("%s", __func__);

    NkIrTypeKind const kind = NkIrType_Procedure;

    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Ir);
        PUSH_VAL(&fp, u8, kind);
        PUSH_VAL(&fp, usize, info.param_types.size);
        nkltype_t const *types_it = info.param_types.data;
        for (usize i = 0; i < info.param_types.size; i++) {
            PUSH_VAL(&fp, u32, (*types_it)->id);
            types_it = (nkltype_t const *)((u8 const *)types_it + info.param_types.stride);
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
                .size = word_size,
                .flags = 0,
                .align = word_size,
                .kind = kind,
                .id = res.id,
            };
            nkltype_t *param_types_copy =
                nk_alloc(nk_arena_getAllocator(&nkl->types.type_arena), info.param_types.size * sizeof(void *));
            nkltype_t const *types_it = info.param_types.data;
            for (usize i = 0; i < info.param_types.size; i++) {
                param_types_copy[i] = *types_it;
                types_it = (nkltype_t const *)((u8 const *)types_it + info.param_types.stride);
            }
            backing->ir_type.as.proc.info.args_t = (NkTypeArray){(nktype_t *)param_types_copy, info.param_types.size};
        } else {
            backing->ir_type = res.type->ir_type;
        }
    }
}

nkltype_t nkl_get_any(NklState nkl, usize word_size) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Any;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            NklField fields[] = {
                {
                    nk_cs2atom("data"),
                    nkl_get_ptr(nkl, word_size, nkl_get_void(nkl), false),
                },
                {
                    nk_cs2atom("type"),
                    nkl_get_typeref(nkl, word_size),
                },
            };
            nkltype_t const underlying_type = nkl_get_struct(nkl, (NklFieldArray){fields, NK_ARRAY_COUNT(fields)});

            res.type->ir_type = underlying_type->ir_type;
            res.type->as = underlying_type->as;
            res.type->tclass = tclass;
            res.type->id = res.id;
            res.type->underlying_type = underlying_type;
        }
    }

    return res.type;
}

nkltype_t nkl_get_array(NklState nkl, nkltype_t elem_type, usize elem_count) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Array;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);
        PUSH_VAL(&fp, u32, elem_type->id);
        PUSH_VAL(&fp, usize, elem_count);

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            nktype_t elem_types[] = {&elem_type->ir_type};
            usize elem_counts[] = {elem_count};

            NkIrAggregateLayout layout = nkir_calcAggregateLayout(
                nk_arena_getAllocator(&nkl->types.tmp_arena), elem_types, elem_counts, 1, 0, 0);
            get_ir_aggregate(nkl, res.type, layout);

            res.type->tclass = tclass;
            res.type->id = res.id;
        }
    }

    return res.type;
}

nkltype_t nkl_get_bool(NklState nkl) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Bool;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            nkltype_t const underlying_type = nkl_get_numeric(nkl, Int8);

            res.type->ir_type = underlying_type->ir_type;
            res.type->as = underlying_type->as;
            res.type->tclass = tclass;
            res.type->id = res.id;
            res.type->underlying_type = underlying_type;
        }
    }

    return res.type;
}

nkltype_t nkl_get_enum(NklState nkl, NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Enum;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);
        PUSH_VAL(&fp, usize, fields.size);
        for (usize i = 0; i < fields.size; i++) {
            PUSH_VAL(&fp, u32, fields.data[i].name);
            PUSH_VAL(&fp, u32, fields.data[i].type->id);
        }

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            NklField enum_fields[] = {
                {
                    .name = nk_cs2atom("data"),
                    .type = nkl_get_union(nkl, fields),
                },
                {
                    .name = nk_cs2atom("tag"),
                    // TODO Hardcoded 64bit enum tag
                    .type = nkl_get_numeric(nkl, Uint64),
                },
            };
            nkltype_t underlying_type = nkl_get_struct(nkl, (NklFieldArray){enum_fields, NK_ARRAY_COUNT(enum_fields)});

            res.type->ir_type = underlying_type->ir_type;
            res.type->as = underlying_type->as;
            res.type->tclass = tclass;
            res.type->id = res.id;
            res.type->underlying_type = underlying_type;
        }
    }

    return res.type;
}

nkltype_t nkl_get_numeric(NklState nkl, NkIrNumericValueType value_type) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Numeric;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);
        PUSH_VAL(&fp, u8, value_type);

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            get_ir_numeric(nkl, res.type, value_type);

            res.type->tclass = tclass;
            res.type->id = res.id;
        }
    }

    return res.type;
}

nkltype_t nkl_get_int(NklState nkl, usize size, bool is_signed) {
    switch (size) {
        case 1:
            return is_signed ? nkl_get_numeric(nkl, Int8) : nkl_get_numeric(nkl, Uint8);
        case 2:
            return is_signed ? nkl_get_numeric(nkl, Int16) : nkl_get_numeric(nkl, Uint16);
        case 4:
            return is_signed ? nkl_get_numeric(nkl, Int32) : nkl_get_numeric(nkl, Uint32);
        case 8:
            return is_signed ? nkl_get_numeric(nkl, Int64) : nkl_get_numeric(nkl, Uint64);

        default:
            nk_assert(!"invalid integer size");
            return NULL;
    }
}

nkltype_t nkl_get_proc(NklState nkl, usize word_size, NklProcInfo info) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Procedure;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);
        PUSH_VAL(&fp, usize, info.param_types.size);
        nkltype_t const *types_it = info.param_types.data;
        for (usize i = 0; i < info.param_types.size; i++) {
            PUSH_VAL(&fp, u32, (*types_it)->id);
            types_it = (nkltype_t const *)((u8 const *)types_it + info.param_types.stride);
        }
        PUSH_VAL(&fp, u32, info.ret_t->id);
        PUSH_VAL(&fp, u8, info.call_conv);
        PUSH_VAL(&fp, u8, info.flags);

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            get_ir_proc(nkl, word_size, res.type, info);

            res.type->tclass = tclass;
            res.type->id = res.id;
        }
    }

    return res.type;
}

nkltype_t nkl_get_ptr(NklState nkl, usize word_size, nkltype_t target_type, bool is_const) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Pointer;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);
        PUSH_VAL(&fp, u32, target_type->id);
        PUSH_VAL(&fp, u8, is_const);

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            get_ir_ptr(nkl, word_size, res.type, &target_type->ir_type);

            res.type->as.ptr.target_type = target_type;
            res.type->as.ptr.is_const = is_const;

            res.type->tclass = tclass;
            res.type->id = res.id;
        }
    }

    return res.type;
}

nkltype_t nkl_get_slice(NklState nkl, usize word_size, nkltype_t target_type, bool is_const) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Slice;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);
        PUSH_VAL(&fp, u32, target_type->id);
        PUSH_VAL(&fp, u8, is_const);

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            NklField fields[] = {
                {
                    nk_cs2atom("data"),
                    nkl_get_ptr(nkl, word_size, target_type, is_const),
                },
                {
                    nk_cs2atom("size"),
                    // TODO Should use word_size
                    nkl_get_numeric(nkl, Uint64),
                },
            };
            nkltype_t const underlying_type = nkl_get_struct(nkl, (NklFieldArray){fields, NK_ARRAY_COUNT(fields)});

            res.type->ir_type = underlying_type->ir_type;
            res.type->as = underlying_type->as;
            res.type->tclass = tclass;
            res.type->id = res.id;
            res.type->underlying_type = underlying_type;
        }
    }

    return res.type;
}

nkltype_t nkl_get_struct(NklState nkl, NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Struct;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);
        PUSH_VAL(&fp, usize, fields.size);
        for (usize i = 0; i < fields.size; i++) {
            PUSH_VAL(&fp, u32, fields.data[i].name);
            PUSH_VAL(&fp, u32, fields.data[i].type->id);
        }

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            nkltype_t const underlying_type =
                nkl_get_tupleEx(nkl, &fields.data->type, fields.size, sizeof(*fields.data));

            nk_slice_copy(nk_arena_getAllocator(&nkl->types.type_arena), &res.type->as.strct.fields, fields);

            res.type->ir_type = underlying_type->ir_type;
            res.type->tclass = tclass;
            res.type->id = res.id;
            res.type->underlying_type = underlying_type;
        }
    }

    return res.type;
}

nkltype_t nkl_get_struct_packed(NklState nkl, NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    (void)nkl;
    (void)fields;
    nk_assert(!"nkl_get_struct_packed is not implemented");
    return NULL;
}

nkltype_t nkl_get_tuple(NklState nkl, NklTypeStridedArray types) {
    return nkl_get_tupleEx(nkl, types.data, types.size, types.stride);
}

nkltype_t nkl_get_tupleEx(NklState nkl, nkltype_t const *types, usize count, usize stride) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Tuple;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);
        PUSH_VAL(&fp, usize, count);

        nkltype_t const *types_it = types;
        for (usize i = 0; i < count; i++) {
            PUSH_VAL(&fp, u32, (*types_it)->id);
            types_it = (nkltype_t const *)((u8 const *)types_it + stride);
        }

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            NkIrAggregateLayout layout = nkir_calcAggregateLayout(
                nk_arena_getAllocator(&nkl->types.tmp_arena), (nktype_t *)types, NULL, count, stride, 0);
            get_ir_aggregate(nkl, res.type, layout);

            res.type->tclass = tclass;
            res.type->id = res.id;
        }
    }

    return res.type;
}

nkltype_t nkl_get_tuple_packed(NklState nkl, NklTypeStridedArray types) {
    NK_LOG_TRC("%s", __func__);

    (void)nkl;
    (void)types;
    nk_assert(!"nkl_get_tuple_packed is not implemented");
    return NULL;
}

nkltype_t nkl_get_typeref(NklState nkl, usize word_size) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Typeref;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            nkltype_t const underlying_type = nkl_get_ptr(nkl, word_size, nkl_get_void(nkl), true);

            res.type->ir_type = underlying_type->ir_type;
            res.type->as = underlying_type->as;
            res.type->tclass = tclass;
            res.type->id = res.id;
            res.type->underlying_type = underlying_type;
        }
    }

    return res.type;
}

nkltype_t nkl_get_union(NklState nkl, NklFieldArray fields) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Union;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);
        PUSH_VAL(&fp, usize, fields.size);
        for (usize i = 0; i < fields.size; i++) {
            PUSH_VAL(&fp, u32, fields.data[i].name);
            PUSH_VAL(&fp, u32, fields.data[i].type->id);
        }

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

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
    }

    return res.type;
}

nkltype_t nkl_get_void(NklState nkl) {
    NK_LOG_TRC("%s", __func__);

    NklTypeClass const tclass = NklType_Tuple;

    TypeSearchResult res;
    NK_ARENA_SCOPE(&nkl->types.tmp_arena) {
        ByteDynArray fp = {NKDA_INIT(nk_arena_getAllocator(&nkl->types.tmp_arena))};
        PUSH_VAL(&fp, u8, TypeSubset_Nkl);
        PUSH_VAL(&fp, u8, tclass);
        PUSH_VAL(&fp, usize, 0);

        res = getTypeByFingerprint(nkl, (ByteArray){NK_SLICE_INIT(fp)}, NULL);

        if (res.inserted) {
            get_ir_aggregate(nkl, res.type, (NkIrAggregateLayout){{0}, 0, 1});

            res.type->tclass = tclass;
            res.type->id = res.id;
        }
    }

    return res.type;
}

void nkl_type_inspect(nkltype_t type, NkStream out) {
    NK_LOG_TRC("%s", __func__);

    if (!type) {
        nk_stream_printf(out, "(null)");
        return;
    }

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
        case NklType_Bool:
            nk_stream_printf(out, "bool");
            break;
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
            nkltype_t target_type = (nkltype_t)type->as.ptr.target_type;
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
                nk_stream_printf(out, "const ");
            }
            nkl_type_inspect((nkltype_t)ptr_t->as.ptr.target_type, out);
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
