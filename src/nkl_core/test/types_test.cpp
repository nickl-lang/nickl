#include "nkl/core/types.h"

#include <gtest/gtest.h>

#include "nkb/common.h"
#include "ntk/log.h"

class types : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});

        nkl_types_init(sizeof(void *));
    }

    void TearDown() override {
        nkl_types_free();
    }
};

TEST_F(types, array) {
    auto f64_t = nkl_get_numeric(Float64);

    auto vec3_t = nkl_get_array(f64_t, 3);

    EXPECT_EQ(vec3_t->id, nkl_get_array(f64_t, 3)->id);
    EXPECT_EQ(vec3_t->ir_type.id, nkl_get_array(f64_t, 3)->ir_type.id);

    EXPECT_EQ(vec3_t->ir_type.size, sizeof(f64) * 3);
    EXPECT_EQ(vec3_t->ir_type.align, sizeof(f64));
    EXPECT_EQ(vec3_t->ir_type.kind, NkIrType_Aggregate);
    ASSERT_EQ(vec3_t->ir_type.as.aggr.elems.size, 1);
    EXPECT_EQ(vec3_t->ir_type.as.aggr.elems.data[0].type->id, f64_t->ir_type.id);
    EXPECT_EQ(vec3_t->tclass, NklType_Array);
    EXPECT_EQ(vec3_t->underlying_type, nullptr);
}

TEST_F(types, numeric) {
    auto i8_t = nkl_get_numeric(Int8);
    auto u32_t = nkl_get_numeric(Uint32);
    auto i64_t = nkl_get_numeric(Int64);
    auto f64_t = nkl_get_numeric(Float64);

    EXPECT_EQ(i8_t->id, nkl_get_numeric(Int8)->id);
    EXPECT_EQ(u32_t->id, nkl_get_numeric(Uint32)->id);
    EXPECT_EQ(i64_t->id, nkl_get_numeric(Int64)->id);
    EXPECT_EQ(f64_t->id, nkl_get_numeric(Float64)->id);

    EXPECT_EQ(i8_t->ir_type.id, nkl_get_numeric(Int8)->ir_type.id);
    EXPECT_EQ(u32_t->ir_type.id, nkl_get_numeric(Uint32)->ir_type.id);
    EXPECT_EQ(i64_t->ir_type.id, nkl_get_numeric(Int64)->ir_type.id);
    EXPECT_EQ(f64_t->ir_type.id, nkl_get_numeric(Float64)->ir_type.id);

    EXPECT_EQ(i8_t->ir_type.size, 1);
    EXPECT_EQ(i8_t->ir_type.align, 1);
    EXPECT_EQ(i8_t->ir_type.kind, NkIrType_Numeric);
    EXPECT_EQ(i8_t->ir_type.as.num.value_type, Int8);
    EXPECT_EQ(i8_t->tclass, NklType_Numeric);
    EXPECT_EQ(i8_t->underlying_type, nullptr);

    EXPECT_EQ(u32_t->ir_type.size, 4);
    EXPECT_EQ(u32_t->ir_type.align, 4);
    EXPECT_EQ(u32_t->ir_type.kind, NkIrType_Numeric);
    EXPECT_EQ(u32_t->ir_type.as.num.value_type, Uint32);
    EXPECT_EQ(u32_t->tclass, NklType_Numeric);
    EXPECT_EQ(u32_t->underlying_type, nullptr);

    EXPECT_EQ(i64_t->ir_type.size, 8);
    EXPECT_EQ(i64_t->ir_type.align, 8);
    EXPECT_EQ(i64_t->ir_type.kind, NkIrType_Numeric);
    EXPECT_EQ(i64_t->ir_type.as.num.value_type, Int64);
    EXPECT_EQ(i64_t->tclass, NklType_Numeric);
    EXPECT_EQ(i64_t->underlying_type, nullptr);

    EXPECT_EQ(f64_t->ir_type.size, 8);
    EXPECT_EQ(f64_t->ir_type.align, 8);
    EXPECT_EQ(f64_t->ir_type.kind, NkIrType_Numeric);
    EXPECT_EQ(f64_t->ir_type.as.num.value_type, Float64);
    EXPECT_EQ(f64_t->tclass, NklType_Numeric);
    EXPECT_EQ(f64_t->underlying_type, nullptr);
}

TEST_F(types, ptr) {
    auto void_t = nkl_get_void();

    auto void_ptr_t = nkl_get_ptr(void_t, false);
    auto str_t = nkl_get_ptr(nkl_get_numeric(Int8), true);

    EXPECT_EQ(void_ptr_t->id, nkl_get_ptr(void_t, false)->id);
    EXPECT_EQ(void_ptr_t->ir_type.id, nkl_get_ptr(void_t, false)->ir_type.id);

    EXPECT_EQ(str_t->id, nkl_get_ptr(nkl_get_numeric(Int8), true)->id);
    EXPECT_EQ(str_t->ir_type.id, nkl_get_ptr(nkl_get_numeric(Int8), true)->ir_type.id);

    auto i8_ptr_t = nkl_get_ptr(nkl_get_numeric(Int8), false);

    EXPECT_NE(str_t->id, i8_ptr_t->id);
    EXPECT_EQ(str_t->ir_type.id, i8_ptr_t->ir_type.id);

    EXPECT_EQ(void_ptr_t->ir_type.size, sizeof(void *));
    EXPECT_EQ(void_ptr_t->ir_type.align, sizeof(void *));
    EXPECT_EQ(void_ptr_t->ir_type.kind, NkIrType_Pointer);
    EXPECT_EQ(void_ptr_t->ir_type.as.ptr.target_type->id, void_t->ir_type.id);
    EXPECT_EQ(void_ptr_t->tclass, NklType_Pointer);
    EXPECT_EQ(void_ptr_t->underlying_type, nullptr);

    EXPECT_EQ(str_t->as.ptr.is_const, true);
    EXPECT_EQ(void_ptr_t->as.ptr.is_const, false);
}

TEST_F(types, tuple) {
    auto f64_t = nkl_get_numeric(Float64);

    nkltype_t types[] = {f64_t, f64_t, f64_t};
    auto vec3_t = nkl_get_tuple({types, NK_ARRAY_COUNT(types)});

    EXPECT_EQ(vec3_t->id, nkl_get_tuple({types, NK_ARRAY_COUNT(types)})->id);
    EXPECT_EQ(vec3_t->ir_type.id, nkl_get_tuple({types, NK_ARRAY_COUNT(types)})->ir_type.id);

    EXPECT_EQ(vec3_t->ir_type.size, sizeof(f64) * 3);
    EXPECT_EQ(vec3_t->ir_type.align, sizeof(f64));
    EXPECT_EQ(vec3_t->ir_type.kind, NkIrType_Aggregate);
    ASSERT_EQ(vec3_t->ir_type.as.aggr.elems.size, 3);
    EXPECT_EQ(vec3_t->ir_type.as.aggr.elems.data[0].type->id, f64_t->ir_type.id);
    EXPECT_EQ(vec3_t->ir_type.as.aggr.elems.data[1].type->id, f64_t->ir_type.id);
    EXPECT_EQ(vec3_t->ir_type.as.aggr.elems.data[2].type->id, f64_t->ir_type.id);
    EXPECT_EQ(vec3_t->tclass, NklType_Tuple);
    EXPECT_EQ(vec3_t->underlying_type, nullptr);
}

TEST_F(types, typeref) {
    auto typeref_t = nkl_get_typeref();
    auto void_ptr_t = nkl_get_ptr(nkl_get_void(), false);

    EXPECT_EQ(typeref_t->id, nkl_get_typeref()->id);
    EXPECT_EQ(typeref_t->ir_type.id, nkl_get_typeref()->ir_type.id);

    EXPECT_EQ(typeref_t->ir_type.size, sizeof(void *));
    EXPECT_EQ(typeref_t->ir_type.align, sizeof(void *));
    EXPECT_EQ(typeref_t->ir_type.kind, NkIrType_Pointer);
    EXPECT_EQ(typeref_t->tclass, NklType_Typeref);
    EXPECT_EQ(typeref_t->underlying_type->id, void_ptr_t->id);
}

TEST_F(types, void) {
    auto void_t = nkl_get_void();

    EXPECT_EQ(void_t->id, nkl_get_void()->id);
    EXPECT_EQ(void_t->ir_type.id, nkl_get_void()->ir_type.id);

    EXPECT_EQ(void_t->ir_type.size, 0);
    EXPECT_EQ(void_t->ir_type.align, 1);
    EXPECT_EQ(void_t->ir_type.kind, NkIrType_Aggregate);
    EXPECT_EQ(void_t->tclass, NklType_Tuple);
    EXPECT_EQ(void_t->underlying_type, nullptr);
}
