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

TEST_F(nkl_types, numeric) {
    auto const i32_t = nkl_type_getNumeric(nkl, Int32);
    auto const i32_t_1 = nkl_type_getNumeric(nkl, Int32);
    auto const i32_t_2 = nkl_type_getNumericDistinct(nkl, Int32);

    EXPECT_EQ(i32_t, i32_t_1);
    EXPECT_NE(i32_t, i32_t_2);

    EXPECT_EQ(i32_t->base.base_t, nullptr);
    EXPECT_EQ(i32_t->base.size, 4);
    EXPECT_EQ(i32_t->base.align, 4);
    EXPECT_EQ(i32_t->base.tclass, NklType_Numeric);

    EXPECT_EQ(i32_t->value_type, Int32);
}

TEST_F(nkl_types, aggregate) {
    auto const i8_t = (NklType)nkl_type_getNumeric(nkl, Int8);
    auto const i16_t = (NklType)nkl_type_getNumeric(nkl, Int16);
    auto const i32_t = (NklType)nkl_type_getNumeric(nkl, Int32);

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

    EXPECT_EQ(agg_t->base.base_t, nullptr);
    EXPECT_EQ(agg_t->base.size, 8);
    EXPECT_EQ(agg_t->base.align, 4);
    EXPECT_EQ(agg_t->base.tclass, NklType_Aggregate);

    ASSERT_EQ(agg_t->types.size, 3);
    EXPECT_EQ(agg_t->types.data[0], i32_t);
    EXPECT_EQ(agg_t->types.data[1], i16_t);
    EXPECT_EQ(agg_t->types.data[2], i8_t);
}

TEST_F(nkl_types, struct) {
    auto const f64_t = (NklType)nkl_type_getNumeric(nkl, Float64);

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
    auto const struct_t = nkl_type_getStruct(nkl, (NklFieldArray){NKS_INIT_STATIC(fields)});
    auto const struct_t_1 = nkl_type_getStruct(nkl, (NklFieldArray){NKS_INIT_STATIC(fields)});
    auto const struct_t_2 = nkl_type_getStructDistinct(nkl, (NklFieldArray){NKS_INIT_STATIC(fields)});

    NklType types[] = {
        f64_t,
        f64_t,
        f64_t,
    };
    auto const base_t = (NklType)nkl_type_getAggregate(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_STATIC(types)});

    EXPECT_EQ(struct_t, struct_t_1);
    EXPECT_NE(struct_t, struct_t_2);

    EXPECT_EQ(struct_t->base.base_t, base_t);
    EXPECT_EQ(struct_t->base.size, 24);
    EXPECT_EQ(struct_t->base.align, 8);
    EXPECT_EQ(struct_t->base.tclass, NklType_Struct);

    ASSERT_EQ(struct_t->fields.size, 3);
    EXPECT_EQ(struct_t->fields.data[0].name, nk_cs2atom("x"));
    EXPECT_EQ(struct_t->fields.data[0].type, f64_t);
    EXPECT_EQ(struct_t->fields.data[1].name, nk_cs2atom("y"));
    EXPECT_EQ(struct_t->fields.data[1].type, f64_t);
    EXPECT_EQ(struct_t->fields.data[2].name, nk_cs2atom("z"));
    EXPECT_EQ(struct_t->fields.data[2].type, f64_t);
}

TEST_F(nkl_types, custom_type_class) {
    auto const vec3_tclass = nkl_type_newClass(nkl);

    auto const f32_t = (NklType)nkl_type_getNumeric(nkl, Float32);

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
    auto const struct_t = (NklType)nkl_type_getStructDistinct(nkl, (NklFieldArray){NKS_INIT_STATIC(fields)});

    auto const vec3_t = nkl_type_getTypeclassInstance(nkl, vec3_tclass, struct_t);

    EXPECT_EQ(vec3_t->base_t, struct_t);
    EXPECT_EQ(vec3_t->size, 12);
    EXPECT_EQ(vec3_t->align, 4);
    EXPECT_EQ(vec3_t->tclass, vec3_tclass);
}
