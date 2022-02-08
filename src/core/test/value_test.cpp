#include "nkl/core/value.hpp"

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"

namespace {

class value : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
        _mctx.tmp_allocator = &m_arena;

        id_init();
        types_init();
    }

    void TearDown() override {
        types_deinit();
        id_deinit();

        m_arena.deinit();
    }

protected:
    ArenaAllocator m_arena;
};

} // namespace

TEST_F(value, any) {
    auto const type = type_get_any();

    EXPECT_EQ(type->typeclass_id, Type_Any);
    EXPECT_EQ(type->size, sizeof(value_t));
    EXPECT_EQ(type->alignment, alignof(value_t));
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"any"});

    EXPECT_EQ(type, type_get_any());
}

TEST_F(value, array) {
    static constexpr size_t c_array_size = 42;

    auto const any_t = type_get_any();
    auto const type = type_get_array(any_t, c_array_size);

    EXPECT_EQ(type->typeclass_id, Type_Array);
    EXPECT_EQ(type->size, c_array_size * any_t->size);
    EXPECT_EQ(type->alignment, any_t->alignment);
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"Array(any, 42)"});

    EXPECT_EQ(type, type_get_array(any_t, c_array_size));

    EXPECT_EQ(type->as.arr_t.elem_type, any_t);
    EXPECT_EQ(type->as.arr_t.elem_count, c_array_size);
}

TEST_F(value, array_ptr) {
    auto const any_t = type_get_any();
    auto const type = type_get_array_ptr(any_t);

    EXPECT_EQ(type->typeclass_id, Type_ArrayPtr);
    EXPECT_EQ(type->size, 16);
    EXPECT_EQ(type->alignment, 8);
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"ArrayPtr(any)"});

    EXPECT_EQ(type, type_get_array_ptr(any_t));

    EXPECT_EQ(type->as.arr_ptr_t.elem_type, any_t);
}

TEST_F(value, bool) {
    auto const type = type_get_bool();

    EXPECT_EQ(type->typeclass_id, Type_Bool);
    EXPECT_EQ(type->size, 1);
    EXPECT_EQ(type->alignment, 1);
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"bool"});

    EXPECT_EQ(type, type_get_bool());
}

TEST_F(value, ptr) {
    auto const void_t = type_get_void();
    auto const type = type_get_ptr(void_t);

    EXPECT_EQ(type->typeclass_id, Type_Ptr);
    EXPECT_EQ(type->size, sizeof(void *));
    EXPECT_EQ(type->alignment, alignof(void *));
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"Ptr(void)"});

    EXPECT_EQ(type, type_get_ptr(void_t));

    auto const type_t = type_get_typeref();
    EXPECT_NE(type, type_get_ptr(type_t));

    EXPECT_EQ(type->as.ptr_t.target_type, void_t);
}

TEST_F(value, string) {
    auto const type = type_get_string();

    EXPECT_EQ(type->typeclass_id, Type_String);
    EXPECT_EQ(type->size, sizeof(string));
    EXPECT_EQ(type->alignment, alignof(string));
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"string"});

    EXPECT_EQ(type, type_get_string());
}

static void _plus(value_t res, value_t a, value_t b) {
    val_as(int64_t, res) = val_as(int64_t, a) + val_as(int64_t, b);
}

TEST_F(value, fn) {
    auto const i64_t = type_get_numeric(Int64);
    type_t params[] = {i64_t, i64_t};
    TypeArray params_ar{params, sizeof(params) / sizeof(void *)};

    auto const type = type_get_fn(i64_t, params_ar, 0, (FuncPtr)_plus, nullptr, false);

    EXPECT_EQ(type->typeclass_id, Type_Fn);
    EXPECT_EQ(type->size, 0);
    EXPECT_EQ(type->alignment, 1);
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"Fn((i64, i64), i64)"});

    EXPECT_EQ(type, type_get_fn(i64_t, params_ar, 0, (FuncPtr)_plus, nullptr, false));

    EXPECT_NE(type, type_get_fn(i64_t, params_ar, 1, (FuncPtr)_plus, nullptr, false));

    EXPECT_EQ(type->as.fn_t.ret_type, i64_t);

    ASSERT_EQ(type->as.fn_t.param_types.size, 2);

    EXPECT_EQ(type->as.fn_t.param_types[0], i64_t);
    EXPECT_EQ(type->as.fn_t.param_types[1], i64_t);
}

TEST_F(value, fn_ptr) {
    auto const i64_t = type_get_numeric(Int64);
    type_t params[] = {i64_t, i64_t};
    TypeArray params_ar{params, sizeof(params) / sizeof(void *)};

    auto const type = type_get_fn_ptr(i64_t, params_ar);

    EXPECT_EQ(type->typeclass_id, Type_FnPtr);
    EXPECT_EQ(type->size, 8);
    EXPECT_EQ(type->alignment, 8);
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"FnPtr((i64, i64), i64)"});

    EXPECT_EQ(type, type_get_fn_ptr(i64_t, params_ar));

    auto const u64_t = type_get_numeric(Uint64);
    EXPECT_NE(type, type_get_fn_ptr(u64_t, params_ar));

    EXPECT_EQ(type->as.fn_t.ret_type, i64_t);

    ASSERT_EQ(type->as.fn_t.param_types.size, 2);

    EXPECT_EQ(type->as.fn_t.param_types[0], i64_t);
    EXPECT_EQ(type->as.fn_t.param_types[1], i64_t);
}

TEST_F(value, numeric) {
    auto const type = type_get_numeric(Int32);

    EXPECT_EQ(type->typeclass_id, Type_Numeric);
    EXPECT_EQ(type->size, 4);
    EXPECT_EQ(type->alignment, 4);
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"i32"});

    EXPECT_EQ(type, type_get_numeric(Int32));
    EXPECT_NE(type, type_get_numeric(Int64));

    auto const test_num = [&](ENumericValueType value_type, size_t size, char const *name) {
        auto const type = type_get_numeric(value_type);

        EXPECT_EQ(type->typeclass_id, Type_Numeric);
        EXPECT_EQ(type->size, size);
        EXPECT_EQ(type->alignment, size);
        EXPECT_EQ(type->qualifiers, 0);

        EXPECT_EQ(std_view(type_name(type)), std::string_view{name});
    };

    test_num(Int8, 1, "i8");
    test_num(Int16, 2, "i16");
    test_num(Int32, 4, "i32");
    test_num(Int64, 8, "i64");
    test_num(Uint8, 1, "u8");
    test_num(Uint16, 2, "u16");
    test_num(Uint32, 4, "u32");
    test_num(Uint64, 8, "u64");
    test_num(Float32, 4, "f32");
    test_num(Float64, 8, "f64");
}

TEST_F(value, struct) {
    Id id_t = cstr_to_id("t");
    Id id_s = cstr_to_id("s");

    NameType fields[] = {{id_t, type_get_typeref()}, {id_s, type_get_symbol()}};
    NameTypeArray fields_ar{fields, sizeof(fields) / sizeof(NameType)};

    auto const type = type_get_struct(fields_ar, 0);

    EXPECT_EQ(type->typeclass_id, Type_Struct);
    EXPECT_EQ(type->size, 16);
    EXPECT_EQ(type->alignment, 8);
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"struct {t: type, s: symbol}"});

    EXPECT_EQ(type, type_get_struct(fields_ar, 0));

    EXPECT_NE(type, type_get_struct(fields_ar, 1));

    ASSERT_EQ(type->as.struct_t.types.size, 2);
    ASSERT_EQ(type->as.struct_t.fields.size, 2);

    EXPECT_EQ(type->as.struct_t.types[0].type, type_get_typeref());
    EXPECT_EQ(type->as.struct_t.types[0].offset, 0);
    EXPECT_EQ(type->as.struct_t.fields[0], id_t);
    EXPECT_EQ(type->as.struct_t.types[1].type, type_get_symbol());
    EXPECT_EQ(type->as.struct_t.types[1].offset, 8);
    EXPECT_EQ(type->as.struct_t.fields[1], id_s);
}

TEST_F(value, symbol) {
    auto const type = type_get_symbol();

    EXPECT_EQ(type->typeclass_id, Type_Symbol);
    EXPECT_EQ(type->size, sizeof(Id));
    EXPECT_EQ(type->alignment, alignof(Id));
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"symbol"});

    EXPECT_EQ(type, type_get_symbol());
}

TEST_F(value, tuple) {
    type_t types[] = {type_get_void(), type_get_typeref(), type_get_symbol()};
    TypeArray types_ar{types, sizeof(types) / sizeof(void *)};

    auto const type = type_get_tuple(types_ar);

    EXPECT_EQ(type->typeclass_id, Type_Tuple);
    EXPECT_EQ(type->size, 16);
    EXPECT_EQ(type->alignment, 8);
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"Tuple(void, type, symbol)"});

    EXPECT_EQ(type, type_get_tuple(types_ar));

    ASSERT_EQ(type->as.tuple_t.types.size, 3);

    EXPECT_EQ(type->as.tuple_t.types[0].type, type_get_void());
    EXPECT_EQ(type->as.tuple_t.types[0].offset, 0);
    EXPECT_EQ(type->as.tuple_t.types[1].type, type_get_typeref());
    EXPECT_EQ(type->as.tuple_t.types[1].offset, 0);
    EXPECT_EQ(type->as.tuple_t.types[2].type, type_get_symbol());
    EXPECT_EQ(type->as.tuple_t.types[2].offset, 8);
}

TEST_F(value, typeref) {
    auto const type = type_get_typeref();

    EXPECT_EQ(type->typeclass_id, Type_Typeref);
    EXPECT_EQ(type->size, sizeof(size_t));
    EXPECT_EQ(type->alignment, alignof(type_t));
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"type"});

    EXPECT_EQ(type, type_get_typeref());
}

TEST_F(value, void) {
    auto const type = type_get_void();

    EXPECT_EQ(type->typeclass_id, Type_Void);
    EXPECT_EQ(type->size, 0);
    EXPECT_EQ(type->alignment, 1);
    EXPECT_EQ(type->qualifiers, 0);
    EXPECT_EQ(std_view(type_name(type)), std::string_view{"void"});

    EXPECT_EQ(type, type_get_void());
}
