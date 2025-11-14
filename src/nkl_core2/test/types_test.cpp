#include "nkl/core/types.h"

#include <gtest/gtest.h>

#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/log.h"
#include "ntk/slice.h"

class nkl_types : public testing::Test {
protected:
    void SetUp() override {
        NK_LOG_INIT({
            .log_level = NkLogLevel_Warning,
            .color_mode = NkLogColorMode_Auto,
        });

        nkl = nkl_newState();
    }

    void TearDown() override {
        defer {
            nkl_freeState(nkl);
        };
    }

    NklState nkl;
};

TEST_F(nkl_types, aggregate) {
    auto const i8_t = nkl_type_getNumeric(nkl, Int8);
    auto const i16_t = nkl_type_getNumeric(nkl, Int16);
    auto const i32_t = nkl_type_getNumeric(nkl, Int32);

    NklType const types[] = {
        i32_t,
        i16_t,
        i8_t,
    };
    auto const agg_t = nkl_type_getAggregate(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_STATIC(types)});
    auto const agg_t_1 = nkl_type_getAggregate(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_STATIC(types)});
    auto const agg_t_2 = nkl_type_getAggregateDistinct(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_STATIC(types)});

    EXPECT_EQ(agg_t, agg_t_1);
    EXPECT_NE(agg_t, agg_t_2);

    EXPECT_EQ(agg_t->base_t, nullptr);
    EXPECT_EQ(agg_t->size, 8);
    EXPECT_EQ(agg_t->align, 4);
    EXPECT_EQ(agg_t->tclass, NklType_Aggregate);

    ASSERT_EQ(agg_t->as.aggr.types.size, 3);
    EXPECT_EQ(agg_t->as.aggr.types.data[0], i32_t);
    EXPECT_EQ(agg_t->as.aggr.types.data[1], i16_t);
    EXPECT_EQ(agg_t->as.aggr.types.data[2], i8_t);
}

TEST_F(nkl_types, array) {
    auto const i32_t = nkl_type_getNumeric(nkl, Int32);

    auto const arr_t = nkl_type_getArray(nkl, i32_t, 5);
    auto const arr_t_1 = nkl_type_getArray(nkl, i32_t, 5);
    auto const arr_t_2 = nkl_type_getArrayDistinct(nkl, i32_t, 5);

    EXPECT_EQ(arr_t, arr_t_1);
    EXPECT_NE(arr_t, arr_t_2);

    EXPECT_EQ(arr_t->base_t, nullptr);
    EXPECT_EQ(arr_t->size, 20);
    EXPECT_EQ(arr_t->align, 4);
    EXPECT_EQ(arr_t->tclass, NklType_Array);

    EXPECT_EQ(arr_t->as.arr.elem_t, i32_t);
    EXPECT_EQ(arr_t->as.arr.count, 5);
}

TEST_F(nkl_types, bool) {
    auto const bool_t = nkl_type_getBool(nkl);
    auto const bool_t_1 = nkl_type_getBool(nkl);
    auto const bool_t_2 = nkl_type_getBoolDistinct(nkl);

    EXPECT_EQ(bool_t, bool_t_1);
    EXPECT_NE(bool_t, bool_t_2);

    EXPECT_EQ(bool_t->base_t, nullptr);
    EXPECT_EQ(bool_t->size, 1);
    EXPECT_EQ(bool_t->align, 1);
    EXPECT_EQ(bool_t->tclass, NklType_Bool);
}

TEST_F(nkl_types, numeric) {
    auto const i32_t = nkl_type_getNumeric(nkl, Int32);
    auto const i32_t_1 = nkl_type_getNumeric(nkl, Int32);
    auto const i32_t_2 = nkl_type_getNumericDistinct(nkl, Int32);

    EXPECT_EQ(i32_t, i32_t_1);
    EXPECT_NE(i32_t, i32_t_2);

    EXPECT_EQ(i32_t->base_t, nullptr);
    EXPECT_EQ(i32_t->size, 4);
    EXPECT_EQ(i32_t->align, 4);
    EXPECT_EQ(i32_t->tclass, NklType_Numeric);

    EXPECT_EQ(i32_t->as.num.value_type, Int32);
}

TEST_F(nkl_types, pointer) {
    auto const i32_t = nkl_type_getNumeric(nkl, Int32);

    auto const ptr_t = nkl_type_getPointer(nkl, 8, i32_t, true);
    auto const ptr_t_1 = nkl_type_getPointer(nkl, 8, i32_t, true);
    auto const ptr_t_2 = nkl_type_getPointerDistinct(nkl, 8, i32_t, true);

    EXPECT_EQ(ptr_t, ptr_t_1);
    EXPECT_NE(ptr_t, ptr_t_2);

    EXPECT_EQ(ptr_t->base_t, nullptr);
    EXPECT_EQ(ptr_t->size, 8);
    EXPECT_EQ(ptr_t->align, 8);
    EXPECT_EQ(ptr_t->tclass, NklType_Pointer);

    EXPECT_EQ(ptr_t->as.ptr.target_t, i32_t);
    EXPECT_EQ(ptr_t->as.ptr.is_const, true);
}

TEST_F(nkl_types, procedure) {
    auto const i32_t = nkl_type_getNumeric(nkl, Int32);

    NklType const param_types[] = {
        i32_t,
        i32_t,
    };
    NklProcInfo const info{
        .param_types = {NKS_INIT_STRIDED_STATIC(param_types)},
        .ret_t = i32_t,
        .flags = 0,
    };

    auto const proc_t = nkl_type_getProcedure(nkl, 8, info);
    auto const proc_t_1 = nkl_type_getProcedure(nkl, 8, info);
    auto const proc_t_2 = nkl_type_getProcedureDistinct(nkl, 8, info);

    EXPECT_EQ(proc_t, proc_t_1);
    EXPECT_NE(proc_t, proc_t_2);

    EXPECT_EQ(proc_t->base_t, nkl_type_getPointer(nkl, 8, nkl_type_getVoid(nkl), true));
    EXPECT_EQ(proc_t->size, 8);
    EXPECT_EQ(proc_t->align, 8);
    EXPECT_EQ(proc_t->tclass, NklType_Procedure);

    ASSERT_EQ(proc_t->as.proc.param_types.size, 2);
    EXPECT_EQ(proc_t->as.proc.param_types.data[0], i32_t);
    EXPECT_EQ(proc_t->as.proc.param_types.data[1], i32_t);
    EXPECT_EQ(proc_t->as.proc.ret_t, i32_t);
    EXPECT_EQ(proc_t->as.proc.flags, 0);
}

TEST_F(nkl_types, struct) {
    auto const f64_t = nkl_type_getNumeric(nkl, Float64);

    NklField const fields[] = {
        (NklField){
            .name = nk_cs2atom("x"),
            .type = f64_t,
        },
        (NklField){
            .name = nk_cs2atom("y"),
            .type = f64_t,
        },
        (NklField){
            .name = nk_cs2atom("z"),
            .type = f64_t,
        },
    };
    auto const struct_t = nkl_type_getStruct(nkl, (NklFieldStridedArray){NKS_INIT_STRIDED_STATIC(fields)});
    auto const struct_t_1 = nkl_type_getStruct(nkl, (NklFieldStridedArray){NKS_INIT_STRIDED_STATIC(fields)});
    auto const struct_t_2 = nkl_type_getStructDistinct(nkl, (NklFieldStridedArray){NKS_INIT_STRIDED_STATIC(fields)});

    NklType types[] = {
        f64_t,
        f64_t,
        f64_t,
    };
    auto const base_t = nkl_type_getAggregate(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_STATIC(types)});

    EXPECT_EQ(struct_t, struct_t_1);
    EXPECT_NE(struct_t, struct_t_2);

    EXPECT_EQ(struct_t->base_t, base_t);
    EXPECT_EQ(struct_t->size, 24);
    EXPECT_EQ(struct_t->align, 8);
    EXPECT_EQ(struct_t->tclass, NklType_Struct);

    ASSERT_EQ(struct_t->as.strct.fields.size, 3);
    EXPECT_EQ(struct_t->as.strct.fields.data[0].name, nk_cs2atom("x"));
    EXPECT_EQ(struct_t->as.strct.fields.data[0].type, f64_t);
    EXPECT_EQ(struct_t->as.strct.fields.data[1].name, nk_cs2atom("y"));
    EXPECT_EQ(struct_t->as.strct.fields.data[1].type, f64_t);
    EXPECT_EQ(struct_t->as.strct.fields.data[2].name, nk_cs2atom("z"));
    EXPECT_EQ(struct_t->as.strct.fields.data[2].type, f64_t);
}

TEST_F(nkl_types, void) {
    auto const void_t = nkl_type_getVoid(nkl);
    auto const void_t_1 = nkl_type_getVoid(nkl);
    auto const void_t_2 = nkl_type_getVoidDistinct(nkl);

    EXPECT_EQ(void_t, void_t_1);
    EXPECT_NE(void_t, void_t_2);

    EXPECT_EQ(void_t->base_t, nullptr);
    EXPECT_EQ(void_t->size, 0);
    EXPECT_EQ(void_t->align, 0);
    EXPECT_EQ(void_t->tclass, NklType_Void);
}

TEST_F(nkl_types, custom_type_class) {
    auto const vec3_tclass = nkl_type_newClass(nkl);

    auto const f32_t = nkl_type_getNumeric(nkl, Float32);

    NklField const fields[] = {
        (NklField){
            .name = nk_cs2atom("x"),
            .type = f32_t,
        },
        (NklField){
            .name = nk_cs2atom("y"),
            .type = f32_t,
        },
        (NklField){
            .name = nk_cs2atom("z"),
            .type = f32_t,
        },
    };
    auto const struct_t = nkl_type_getStructDistinct(nkl, (NklFieldStridedArray){NKS_INIT_STRIDED_STATIC(fields)});

    auto const vec3_t = nkl_type_getTypeclassInstance(nkl, vec3_tclass, struct_t);

    EXPECT_EQ(vec3_t->base_t, struct_t);
    EXPECT_EQ(vec3_t->size, 12);
    EXPECT_EQ(vec3_t->align, 4);
    EXPECT_EQ(vec3_t->tclass, vec3_tclass);
}
