#include "nkl/core/types.h"

#include <gtest/gtest.h>

#include "nkb/common.h"
#include "nkl/core/nickl.h"
#include "ntk/common.h"
#include "ntk/log.h"
#include "ntk/string_builder.h"

class types : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});

        nkl = nkl_state_create({}, {});
    }

    void TearDown() override {
        nkl_state_free(nkl);
    }

protected:
    NklState nkl{};
};

static constexpr usize word_size = sizeof(void *);

TEST_F(types, any) {
    auto any_t = nkl_get_any(nkl, word_size);

    NklField fields[] = {
        {nk_cs2atom("data"), nkl_get_ptr(nkl, word_size, nkl_get_void(nkl), false)},
        {nk_cs2atom("type"), nkl_get_typeref(nkl, word_size)},
    };
    auto struct_t = nkl_get_struct(nkl, {fields, NK_ARRAY_COUNT(fields)});

    EXPECT_EQ(any_t->id, nkl_get_any(nkl, word_size)->id);
    EXPECT_EQ(any_t->ir_type.id, nkl_get_any(nkl, word_size)->ir_type.id);

    EXPECT_EQ(any_t->tclass, NklType_Any);
    ASSERT_TRUE(any_t->underlying_type);
    EXPECT_EQ(any_t->underlying_type->id, struct_t->id);
    ASSERT_EQ(any_t->ir_type.id, struct_t->ir_type.id);

    NKSB_FIXED_BUFFER(sb, 64);
    nkl_type_inspect(any_t, nksb_getStream(&sb));
    EXPECT_EQ("any_t", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, array) {
    auto f64_t = nkl_get_numeric(nkl, Float64);

    auto get_vec3_t = [&]() {
        return nkl_get_array(nkl, f64_t, 3);
    };

    auto vec3_t = get_vec3_t();

    EXPECT_EQ(vec3_t->id, get_vec3_t()->id);
    EXPECT_EQ(vec3_t->ir_type.id, get_vec3_t()->ir_type.id);

    EXPECT_EQ(vec3_t->ir_type.size, sizeof(f64) * 3);
    EXPECT_EQ(vec3_t->ir_type.align, alignof(f64));
    ASSERT_EQ(vec3_t->ir_type.kind, NkIrType_Aggregate);
    ASSERT_EQ(vec3_t->ir_type.as.aggr.elems.size, 1);
    EXPECT_EQ(vec3_t->ir_type.as.aggr.elems.data[0].type->id, f64_t->ir_type.id);
    EXPECT_EQ(vec3_t->tclass, NklType_Array);
    EXPECT_EQ(vec3_t->underlying_type, nullptr);

    NKSB_FIXED_BUFFER(sb, 64);
    nkl_type_inspect(vec3_t, nksb_getStream(&sb));
    EXPECT_EQ("[3]f64", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, bool) {
    auto bool_t = nkl_get_bool(nkl);

    auto i8_t = nkl_get_numeric(nkl, Int8);

    EXPECT_EQ(bool_t->id, nkl_get_bool(nkl)->id);
    EXPECT_EQ(bool_t->ir_type.id, nkl_get_bool(nkl)->ir_type.id);

    EXPECT_EQ(bool_t->tclass, NklType_Bool);
    ASSERT_TRUE(bool_t->underlying_type);
    EXPECT_EQ(bool_t->underlying_type->id, i8_t->id);
    ASSERT_EQ(bool_t->ir_type.id, i8_t->ir_type.id);

    NKSB_FIXED_BUFFER(sb, 64);
    nkl_type_inspect(bool_t, nksb_getStream(&sb));
    EXPECT_EQ("bool", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, enum) {
    auto i32_t = nkl_get_numeric(nkl, Int32);

    NklField fields[] = {
        {nk_cs2atom("val"), i32_t},
        {nk_cs2atom("nil"), nkl_get_void(nkl)},
    };

    auto get_opt_t = [&]() {
        return nkl_get_enum(nkl, {fields, NK_ARRAY_COUNT(fields)});
    };

    auto opt_t = get_opt_t();

    EXPECT_EQ(opt_t->id, get_opt_t()->id);
    EXPECT_EQ(opt_t->ir_type.id, get_opt_t()->ir_type.id);

    auto union_t = nkl_get_union(nkl, {fields, NK_ARRAY_COUNT(fields)});

    NklField struct_fields[] = {
        {nk_cs2atom("data"), union_t},
        {nk_cs2atom("tag"), nkl_get_numeric(nkl, Uint64)},
    };
    auto struct_t = nkl_get_struct(nkl, {struct_fields, NK_ARRAY_COUNT(struct_fields)});

    EXPECT_EQ(opt_t->tclass, NklType_Enum);
    ASSERT_TRUE(opt_t->underlying_type);
    EXPECT_EQ(opt_t->underlying_type->id, struct_t->id);
    ASSERT_EQ(opt_t->ir_type.id, struct_t->ir_type.id);

    NKSB_FIXED_BUFFER(sb, 64);
    nkl_type_inspect(opt_t, nksb_getStream(&sb));
    EXPECT_EQ("enum { val: i32, nil: void, }", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, numeric) {
    auto i8_t = nkl_get_numeric(nkl, Int8);
    auto u32_t = nkl_get_numeric(nkl, Uint32);
    auto i64_t = nkl_get_numeric(nkl, Int64);
    auto f64_t = nkl_get_numeric(nkl, Float64);

    EXPECT_EQ(i8_t->id, nkl_get_numeric(nkl, Int8)->id);
    EXPECT_EQ(u32_t->id, nkl_get_numeric(nkl, Uint32)->id);
    EXPECT_EQ(i64_t->id, nkl_get_numeric(nkl, Int64)->id);
    EXPECT_EQ(f64_t->id, nkl_get_numeric(nkl, Float64)->id);

    EXPECT_EQ(i8_t->ir_type.id, nkl_get_numeric(nkl, Int8)->ir_type.id);
    EXPECT_EQ(u32_t->ir_type.id, nkl_get_numeric(nkl, Uint32)->ir_type.id);
    EXPECT_EQ(i64_t->ir_type.id, nkl_get_numeric(nkl, Int64)->ir_type.id);
    EXPECT_EQ(f64_t->ir_type.id, nkl_get_numeric(nkl, Float64)->ir_type.id);

    NKSB_FIXED_BUFFER(sb, 64);

    EXPECT_EQ(i8_t->ir_type.size, 1);
    EXPECT_EQ(i8_t->ir_type.align, 1);
    ASSERT_EQ(i8_t->ir_type.kind, NkIrType_Numeric);
    EXPECT_EQ(i8_t->ir_type.as.num.value_type, Int8);
    EXPECT_EQ(i8_t->tclass, NklType_Numeric);
    EXPECT_EQ(i8_t->underlying_type, nullptr);

    nksb_clear(&sb);
    nkl_type_inspect(i8_t, nksb_getStream(&sb));
    EXPECT_EQ("i8", nk_s2stdStr({NKS_INIT(sb)}));

    EXPECT_EQ(u32_t->ir_type.size, 4);
    EXPECT_EQ(u32_t->ir_type.align, 4);
    ASSERT_EQ(u32_t->ir_type.kind, NkIrType_Numeric);
    EXPECT_EQ(u32_t->ir_type.as.num.value_type, Uint32);
    EXPECT_EQ(u32_t->tclass, NklType_Numeric);
    EXPECT_EQ(u32_t->underlying_type, nullptr);

    nksb_clear(&sb);
    nkl_type_inspect(u32_t, nksb_getStream(&sb));
    EXPECT_EQ("u32", nk_s2stdStr({NKS_INIT(sb)}));

    EXPECT_EQ(i64_t->ir_type.size, 8);
    EXPECT_EQ(i64_t->ir_type.align, 8);
    ASSERT_EQ(i64_t->ir_type.kind, NkIrType_Numeric);
    EXPECT_EQ(i64_t->ir_type.as.num.value_type, Int64);
    EXPECT_EQ(i64_t->tclass, NklType_Numeric);
    EXPECT_EQ(i64_t->underlying_type, nullptr);

    nksb_clear(&sb);
    nkl_type_inspect(i64_t, nksb_getStream(&sb));
    EXPECT_EQ("i64", nk_s2stdStr({NKS_INIT(sb)}));

    EXPECT_EQ(f64_t->ir_type.size, 8);
    EXPECT_EQ(f64_t->ir_type.align, 8);
    ASSERT_EQ(f64_t->ir_type.kind, NkIrType_Numeric);
    EXPECT_EQ(f64_t->ir_type.as.num.value_type, Float64);
    EXPECT_EQ(f64_t->tclass, NklType_Numeric);
    EXPECT_EQ(f64_t->underlying_type, nullptr);

    nksb_clear(&sb);
    nkl_type_inspect(f64_t, nksb_getStream(&sb));
    EXPECT_EQ("f64", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, proc) {
    auto i32_t = nkl_get_numeric(nkl, Int32);
    auto get_add_t = [&]() {
        nkltype_t add_params[] = {i32_t, i32_t};
        return nkl_get_proc(
            nkl,
            word_size,
            NklProcInfo{
                .param_types = {add_params, NK_ARRAY_COUNT(add_params)},
                .ret_t = i32_t,
                .call_conv = NkCallConv_Nk,
                .flags = 0,
            });
    };

    auto add_t = get_add_t();

    EXPECT_EQ(add_t->id, get_add_t()->id);
    EXPECT_EQ(add_t->ir_type.id, get_add_t()->ir_type.id);

    EXPECT_EQ(add_t->ir_type.size, sizeof(void *));
    EXPECT_EQ(add_t->ir_type.align, alignof(void *));
    ASSERT_EQ(add_t->ir_type.kind, NkIrType_Procedure);
    ASSERT_EQ(add_t->ir_type.as.proc.info.args_t.size, 2);
    EXPECT_EQ(add_t->ir_type.as.proc.info.args_t.data[0]->id, i32_t->ir_type.id);
    EXPECT_EQ(add_t->ir_type.as.proc.info.args_t.data[1]->id, i32_t->ir_type.id);
    EXPECT_EQ(add_t->ir_type.as.proc.info.ret_t->id, i32_t->ir_type.id);
    EXPECT_EQ(add_t->ir_type.as.proc.info.call_conv, NkCallConv_Nk);
    EXPECT_EQ(add_t->ir_type.as.proc.info.flags, 0);
    EXPECT_EQ(add_t->tclass, NklType_Procedure);
    EXPECT_EQ(add_t->underlying_type, nullptr);

    NKSB_FIXED_BUFFER(sb, 64);
    nkl_type_inspect(add_t, nksb_getStream(&sb));
    EXPECT_EQ("(i32, i32) -> i32", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, ptr) {
    auto void_t = nkl_get_void(nkl);
    auto i8_t = nkl_get_numeric(nkl, Int8);

    auto get_void_ptr_t = [&]() {
        return nkl_get_ptr(nkl, word_size, void_t, false);
    };

    auto get_str_t = [&]() {
        return nkl_get_ptr(nkl, word_size, i8_t, true);
    };

    auto void_ptr_t = get_void_ptr_t();
    auto str_t = get_str_t();

    EXPECT_EQ(void_ptr_t->id, get_void_ptr_t()->id);
    EXPECT_EQ(void_ptr_t->ir_type.id, get_void_ptr_t()->ir_type.id);

    EXPECT_EQ(str_t->id, get_str_t()->id);
    EXPECT_EQ(str_t->ir_type.id, get_str_t()->ir_type.id);

    auto i8_ptr_t = nkl_get_ptr(nkl, word_size, i8_t, false);

    EXPECT_NE(str_t->id, i8_ptr_t->id);
    EXPECT_EQ(str_t->ir_type.id, i8_ptr_t->ir_type.id);

    EXPECT_EQ(void_ptr_t->ir_type.size, sizeof(void *));
    EXPECT_EQ(void_ptr_t->ir_type.align, alignof(void *));
    ASSERT_EQ(void_ptr_t->ir_type.kind, NkIrType_Pointer);
    EXPECT_EQ(void_ptr_t->tclass, NklType_Pointer);
    EXPECT_EQ(void_ptr_t->underlying_type, nullptr);

    EXPECT_EQ(str_t->as.ptr.is_const, true);
    EXPECT_EQ(void_ptr_t->as.ptr.is_const, false);

    NKSB_FIXED_BUFFER(sb, 64);

    nksb_clear(&sb);
    nkl_type_inspect(void_ptr_t, nksb_getStream(&sb));
    EXPECT_EQ("*void", nk_s2stdStr({NKS_INIT(sb)}));

    nksb_clear(&sb);
    nkl_type_inspect(str_t, nksb_getStream(&sb));
    EXPECT_EQ("*const i8", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, slice) {
    auto i8_t = nkl_get_numeric(nkl, Int8);

    auto get_string_t = [&]() {
        return nkl_get_slice(nkl, word_size, i8_t, true);
    };

    auto string_t = get_string_t();

    NklField fields[] = {
        {nk_cs2atom("data"), nkl_get_ptr(nkl, word_size, i8_t, true)},
        {nk_cs2atom("size"), nkl_get_numeric(nkl, Uint64)},
    };
    auto struct_t = nkl_get_struct(nkl, {fields, NK_ARRAY_COUNT(fields)});

    EXPECT_EQ(string_t->id, get_string_t()->id);
    EXPECT_EQ(string_t->ir_type.id, get_string_t()->ir_type.id);

    EXPECT_EQ(string_t->tclass, NklType_Slice);
    ASSERT_TRUE(string_t->underlying_type);
    EXPECT_EQ(string_t->underlying_type->id, struct_t->id);
    ASSERT_EQ(string_t->ir_type.id, struct_t->ir_type.id);

    NKSB_FIXED_BUFFER(sb, 64);
    nkl_type_inspect(string_t, nksb_getStream(&sb));
    EXPECT_EQ("[]const i8", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, struct) {
    auto i64_t = nkl_get_numeric(nkl, Int64);

    auto get_ivec2_t = [&]() {
        NklField fields[] = {
            {nk_cs2atom("x"), i64_t},
            {nk_cs2atom("y"), i64_t},
        };
        return nkl_get_struct(nkl, {fields, NK_ARRAY_COUNT(fields)});
    };

    auto ivec2_t = get_ivec2_t();

    EXPECT_EQ(ivec2_t->id, get_ivec2_t()->id);
    EXPECT_EQ(ivec2_t->ir_type.id, get_ivec2_t()->ir_type.id);

    nkltype_t types[] = {i64_t, i64_t};
    auto tuple_t = nkl_get_tuple(nkl, {types, NK_ARRAY_COUNT(types)});

    ASSERT_EQ(ivec2_t->as.strct.fields.size, 2);
    ASSERT_EQ(ivec2_t->as.strct.fields.data[0].name, nk_cs2atom("x"));
    ASSERT_EQ(ivec2_t->as.strct.fields.data[1].name, nk_cs2atom("y"));
    EXPECT_EQ(ivec2_t->tclass, NklType_Struct);
    ASSERT_TRUE(ivec2_t->underlying_type);
    EXPECT_EQ(ivec2_t->underlying_type->id, tuple_t->id);
    ASSERT_EQ(ivec2_t->ir_type.id, tuple_t->ir_type.id);

    NKSB_FIXED_BUFFER(sb, 64);
    nkl_type_inspect(ivec2_t, nksb_getStream(&sb));
    EXPECT_EQ("struct { x: i64, y: i64, }", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, tuple) {
    auto f64_t = nkl_get_numeric(nkl, Float64);

    auto get_vec3_t = [&]() {
        nkltype_t types[] = {f64_t, f64_t, f64_t};
        return nkl_get_tuple(nkl, {types, NK_ARRAY_COUNT(types)});
    };

    auto vec3_t = get_vec3_t();

    EXPECT_EQ(vec3_t->id, get_vec3_t()->id);
    EXPECT_EQ(vec3_t->ir_type.id, get_vec3_t()->ir_type.id);

    EXPECT_EQ(vec3_t->ir_type.size, sizeof(f64) * 3);
    EXPECT_EQ(vec3_t->ir_type.align, alignof(f64));
    ASSERT_EQ(vec3_t->ir_type.kind, NkIrType_Aggregate);
    ASSERT_EQ(vec3_t->ir_type.as.aggr.elems.size, 3);
    EXPECT_EQ(vec3_t->ir_type.as.aggr.elems.data[0].type->id, f64_t->ir_type.id);
    EXPECT_EQ(vec3_t->ir_type.as.aggr.elems.data[1].type->id, f64_t->ir_type.id);
    EXPECT_EQ(vec3_t->ir_type.as.aggr.elems.data[2].type->id, f64_t->ir_type.id);
    EXPECT_EQ(vec3_t->tclass, NklType_Tuple);
    EXPECT_EQ(vec3_t->underlying_type, nullptr);

    NKSB_FIXED_BUFFER(sb, 64);

    nksb_clear(&sb);
    nkl_type_inspect(vec3_t, nksb_getStream(&sb));
    EXPECT_EQ("(f64, f64, f64, )", nk_s2stdStr({NKS_INIT(sb)}));

    auto void_t = nkl_get_tuple(nkl, {});

    EXPECT_EQ(void_t->ir_type.size, 0);
    EXPECT_EQ(void_t->ir_type.align, 1);
    ASSERT_EQ(void_t->ir_type.kind, NkIrType_Aggregate);
    EXPECT_EQ(void_t->ir_type.as.aggr.elems.size, 0);
    EXPECT_EQ(void_t->tclass, NklType_Tuple);
    EXPECT_EQ(void_t->underlying_type, nullptr);

    nksb_clear(&sb);
    nkl_type_inspect(void_t, nksb_getStream(&sb));
    EXPECT_EQ("void", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, typeref) {
    auto typeref_t = nkl_get_typeref(nkl, word_size);
    auto void_ptr_t = nkl_get_ptr(nkl, word_size, nkl_get_void(nkl), true);

    EXPECT_EQ(typeref_t->id, nkl_get_typeref(nkl, word_size)->id);
    EXPECT_EQ(typeref_t->ir_type.id, nkl_get_typeref(nkl, word_size)->ir_type.id);

    EXPECT_EQ(typeref_t->ir_type.size, sizeof(void *));
    EXPECT_EQ(typeref_t->ir_type.align, alignof(void *));
    EXPECT_EQ(typeref_t->ir_type.kind, NkIrType_Pointer);
    EXPECT_EQ(typeref_t->tclass, NklType_Typeref);
    ASSERT_TRUE(typeref_t->underlying_type);
    EXPECT_EQ(typeref_t->underlying_type->id, void_ptr_t->id);
    ASSERT_EQ(typeref_t->ir_type.id, void_ptr_t->ir_type.id);

    NKSB_FIXED_BUFFER(sb, 64);
    nkl_type_inspect(typeref_t, nksb_getStream(&sb));
    EXPECT_EQ("type_t", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, union) {
    auto f64_t = nkl_get_numeric(nkl, Float64);
    auto u64_t = nkl_get_numeric(nkl, Uint64);

    auto get_union_t = [&]() {
        NklField fields[] = {
            {nk_cs2atom("float64"), f64_t},
            {nk_cs2atom("uint64"), u64_t},
        };
        return nkl_get_union(nkl, {fields, NK_ARRAY_COUNT(fields)});
    };

    auto union_t = get_union_t();

    EXPECT_EQ(union_t->id, get_union_t()->id);
    EXPECT_EQ(union_t->ir_type.id, get_union_t()->ir_type.id);

    EXPECT_EQ(union_t->ir_type.size, sizeof(f64));
    EXPECT_EQ(union_t->ir_type.align, alignof(f64));
    ASSERT_EQ(union_t->ir_type.kind, NkIrType_Numeric);
    EXPECT_EQ(union_t->ir_type.as.num.value_type, Float64);
    ASSERT_EQ(union_t->as.strct.fields.size, 2);
    ASSERT_EQ(union_t->as.strct.fields.data[0].name, nk_cs2atom("float64"));
    ASSERT_EQ(union_t->as.strct.fields.data[1].name, nk_cs2atom("uint64"));
    EXPECT_EQ(union_t->tclass, NklType_Union);

    NKSB_FIXED_BUFFER(sb, 64);
    nkl_type_inspect(union_t, nksb_getStream(&sb));
    EXPECT_EQ("union { float64: f64, uint64: u64, }", nk_s2stdStr({NKS_INIT(sb)}));
}

TEST_F(types, void) {
    auto void_t = nkl_get_void(nkl);

    EXPECT_EQ(void_t->id, nkl_get_void(nkl)->id);
    EXPECT_EQ(void_t->ir_type.id, nkl_get_void(nkl)->ir_type.id);

    EXPECT_EQ(void_t->ir_type.size, 0);
    EXPECT_EQ(void_t->ir_type.align, 1);
    EXPECT_EQ(void_t->ir_type.kind, NkIrType_Aggregate);
    EXPECT_EQ(void_t->tclass, NklType_Tuple);
    EXPECT_EQ(void_t->underlying_type, nullptr);

    NKSB_FIXED_BUFFER(sb, 64);
    nkl_type_inspect(void_t, nksb_getStream(&sb));
    EXPECT_EQ("void", nk_s2stdStr({NKS_INIT(sb)}));
}
