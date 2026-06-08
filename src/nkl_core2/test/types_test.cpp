#include "nkl/core/types.h"

#include <gtest/gtest.h>

#include "nkl/core/nickl.h"
#include "ntk/atom.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/string_builder.h"

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
    auto const type = nkl_type_getAggregate(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_STATIC(types)});
    auto const type1 = nkl_type_getAggregate(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_STATIC(types)});
    auto const type2 = nkl_type_getAggregateDistinct(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_STATIC(types)});

    EXPECT_EQ(type, type1);
    EXPECT_NE(type, type2);

    EXPECT_EQ(type->base_t, nullptr);
    EXPECT_EQ(type->size, 8);
    EXPECT_EQ(type->align, 4);
    EXPECT_EQ(type->tclass, NklType_Aggregate);

    ASSERT_EQ(type->as.aggr.types.size, 3);
    EXPECT_EQ(type->as.aggr.types.data[0], i32_t);
    EXPECT_EQ(type->as.aggr.types.data[1], i16_t);
    EXPECT_EQ(type->as.aggr.types.data[2], i8_t);

    NKSB_FIXED_BUFFER(str, 64);
    nkl_type_inspect(nksb_getStream(&str), type);
    EXPECT_EQ("(i32, i16, i8)", nk_s2std({NKS_INIT(str)}));
}

TEST_F(nkl_types, array) {
    auto const i32_t = nkl_type_getNumeric(nkl, Int32);

    auto const type = nkl_type_getArray(nkl, i32_t, 5);
    auto const type1 = nkl_type_getArray(nkl, i32_t, 5);
    auto const type2 = nkl_type_getArrayDistinct(nkl, i32_t, 5);

    EXPECT_EQ(type, type1);
    EXPECT_NE(type, type2);

    EXPECT_EQ(type->base_t, nullptr);
    EXPECT_EQ(type->size, 20);
    EXPECT_EQ(type->align, 4);
    EXPECT_EQ(type->tclass, NklType_Array);

    EXPECT_EQ(type->as.arr.elem_t, i32_t);
    EXPECT_EQ(type->as.arr.count, 5);

    NKSB_FIXED_BUFFER(str, 64);
    nkl_type_inspect(nksb_getStream(&str), type);
    EXPECT_EQ("[5]i32", nk_s2std({NKS_INIT(str)}));
}

TEST_F(nkl_types, bool) {
    auto const type = nkl_type_getBool(nkl);
    auto const type1 = nkl_type_getBool(nkl);
    auto const type2 = nkl_type_getBoolDistinct(nkl);

    EXPECT_EQ(type, type1);
    EXPECT_NE(type, type2);

    EXPECT_EQ(type->base_t, nullptr);
    EXPECT_EQ(type->size, 1);
    EXPECT_EQ(type->align, 1);
    EXPECT_EQ(type->tclass, NklType_Bool);

    NKSB_FIXED_BUFFER(str, 64);
    nkl_type_inspect(nksb_getStream(&str), type);
    EXPECT_EQ("bool", nk_s2std({NKS_INIT(str)}));
}

TEST_F(nkl_types, numeric) {
    auto const type = nkl_type_getNumeric(nkl, Int32);
    auto const type1 = nkl_type_getNumeric(nkl, Int32);
    auto const type2 = nkl_type_getNumericDistinct(nkl, Int32);

    EXPECT_EQ(type, type1);
    EXPECT_NE(type, type2);

    EXPECT_EQ(type->base_t, nullptr);
    EXPECT_EQ(type->size, 4);
    EXPECT_EQ(type->align, 4);
    EXPECT_EQ(type->tclass, NklType_Numeric);

    EXPECT_EQ(type->as.num.value_type, Int32);

    NKSB_FIXED_BUFFER(str, 64);
    nkl_type_inspect(nksb_getStream(&str), type);
    EXPECT_EQ("i32", nk_s2std({NKS_INIT(str)}));
}

TEST_F(nkl_types, pointer) {
    auto const i32_t = nkl_type_getNumeric(nkl, Int32);

    auto const type = nkl_type_getPointer(nkl, 8, i32_t, true);
    auto const type1 = nkl_type_getPointer(nkl, 8, i32_t, true);
    auto const type2 = nkl_type_getPointerDistinct(nkl, 8, i32_t, true);

    EXPECT_EQ(type, type1);
    EXPECT_NE(type, type2);

    EXPECT_EQ(type->base_t, nullptr);
    EXPECT_EQ(type->size, 8);
    EXPECT_EQ(type->align, 8);
    EXPECT_EQ(type->tclass, NklType_Pointer);

    EXPECT_EQ(type->as.ptr.target_t, i32_t);
    EXPECT_EQ(type->as.ptr.is_const, true);

    NKSB_FIXED_BUFFER(str, 64);
    nkl_type_inspect(nksb_getStream(&str), type);
    EXPECT_EQ("*const i32", nk_s2std({NKS_INIT(str)}));
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

    auto const type = nkl_type_getProcedure(nkl, 8, info);
    auto const type1 = nkl_type_getProcedure(nkl, 8, info);
    auto const type2 = nkl_type_getProcedureDistinct(nkl, 8, info);

    EXPECT_EQ(type, type1);
    EXPECT_NE(type, type2);

    EXPECT_EQ(type->base_t, nkl_type_getPointer(nkl, 8, nkl_type_getVoid(nkl), true));
    EXPECT_EQ(type->size, 8);
    EXPECT_EQ(type->align, 8);
    EXPECT_EQ(type->tclass, NklType_Procedure);

    ASSERT_EQ(type->as.proc.param_types.size, 2);
    EXPECT_EQ(type->as.proc.param_types.data[0], i32_t);
    EXPECT_EQ(type->as.proc.param_types.data[1], i32_t);
    EXPECT_EQ(type->as.proc.ret_t, i32_t);
    EXPECT_EQ(type->as.proc.flags, 0);

    NKSB_FIXED_BUFFER(str, 64);
    nkl_type_inspect(nksb_getStream(&str), type);
    EXPECT_EQ("*proc (i32, i32) i32", nk_s2std({NKS_INIT(str)}));
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
    auto const type = nkl_type_getStruct(nkl, (NklFieldStridedArray){NKS_INIT_STRIDED_STATIC(fields)});
    auto const type1 = nkl_type_getStruct(nkl, (NklFieldStridedArray){NKS_INIT_STRIDED_STATIC(fields)});
    auto const type2 = nkl_type_getStructDistinct(nkl, (NklFieldStridedArray){NKS_INIT_STRIDED_STATIC(fields)});

    NklType types[] = {
        f64_t,
        f64_t,
        f64_t,
    };
    auto const base_t = nkl_type_getAggregate(nkl, (NklTypeStridedArray){NKS_INIT_STRIDED_STATIC(types)});

    EXPECT_EQ(type, type1);
    EXPECT_NE(type, type2);

    EXPECT_EQ(type->base_t, base_t);
    EXPECT_EQ(type->size, 24);
    EXPECT_EQ(type->align, 8);
    EXPECT_EQ(type->tclass, NklType_Struct);

    ASSERT_EQ(type->as.strct.fields.size, 3);
    EXPECT_EQ(type->as.strct.fields.data[0].name, nk_cs2atom("x"));
    EXPECT_EQ(type->as.strct.fields.data[0].type, f64_t);
    EXPECT_EQ(type->as.strct.fields.data[1].name, nk_cs2atom("y"));
    EXPECT_EQ(type->as.strct.fields.data[1].type, f64_t);
    EXPECT_EQ(type->as.strct.fields.data[2].name, nk_cs2atom("z"));
    EXPECT_EQ(type->as.strct.fields.data[2].type, f64_t);

    NKSB_FIXED_BUFFER(str, 64);
    nkl_type_inspect(nksb_getStream(&str), type);
    EXPECT_EQ("struct {x: f64, y: f64, z: f64}", nk_s2std({NKS_INIT(str)}));
}

TEST_F(nkl_types, typeref) {
    auto const type = nkl_type_getTyperef(nkl, 8);
    auto const type1 = nkl_type_getTyperef(nkl, 8);
    auto const type2 = nkl_type_getTyperefDistinct(nkl, 8);

    EXPECT_EQ(type, type1);
    EXPECT_NE(type, type2);

    EXPECT_EQ(type->base_t, nullptr);
    EXPECT_EQ(type->size, 8);
    EXPECT_EQ(type->align, 8);
    EXPECT_EQ(type->tclass, NklType_Typeref);

    NKSB_FIXED_BUFFER(str, 64);
    nkl_type_inspect(nksb_getStream(&str), type);
    EXPECT_EQ("type_t", nk_s2std({NKS_INIT(str)}));
}

TEST_F(nkl_types, void) {
    auto const type = nkl_type_getVoid(nkl);
    auto const type1 = nkl_type_getVoid(nkl);
    auto const type2 = nkl_type_getVoidDistinct(nkl);

    EXPECT_EQ(type, type1);
    EXPECT_NE(type, type2);

    EXPECT_EQ(type->base_t, nullptr);
    EXPECT_EQ(type->size, 0);
    EXPECT_EQ(type->align, 0);
    EXPECT_EQ(type->tclass, NklType_Void);

    NKSB_FIXED_BUFFER(str, 64);
    nkl_type_inspect(nksb_getStream(&str), type);
    EXPECT_EQ("void", nk_s2std({NKS_INIT(str)}));
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
