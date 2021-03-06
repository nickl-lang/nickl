#include "nk/vm/value.hpp"

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.h"
#include "nk/common/utils.hpp"

using namespace nk::vm;

namespace {

class value : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
        _mctx.tmp_allocator = &m_arena;

        types_init();
    }

    void TearDown() override {
        types_deinit();

        m_arena.deinit();
    }

protected:
    ArenaAllocator m_arena;
};

} // namespace

TEST_F(value, array) {
    static constexpr size_t c_array_size = 42;

    auto const i32_t = type_get_numeric(Int32);
    auto const type = type_get_array(i32_t, c_array_size);

    EXPECT_EQ(type->typeclass_id, Type_Array);
    EXPECT_EQ(type->size, c_array_size * i32_t->size);
    EXPECT_EQ(type->alignment, i32_t->alignment);
    EXPECT_EQ(std_str(type_name(type)), "array{i32, 42}");

    EXPECT_EQ(type, type_get_array(i32_t, c_array_size));

    EXPECT_EQ(type_array_elemType(type), i32_t);
    EXPECT_EQ(type_array_size(type), c_array_size);
}

TEST_F(value, ptr) {
    auto const void_t = type_get_void();
    auto const type = type_get_ptr(void_t);

    EXPECT_EQ(type->typeclass_id, Type_Ptr);
    EXPECT_EQ(type->size, sizeof(void *));
    EXPECT_EQ(type->alignment, alignof(void *));
    EXPECT_EQ(std_str(type_name(type)), "ptr{void}");

    EXPECT_EQ(type, type_get_ptr(void_t));

    auto const type_t = type_get_typeref();
    EXPECT_NE(type, type_get_ptr(type_t));

    EXPECT_EQ(type->as.ptr.target_type, void_t);
}

TEST_F(value, fn) {
    static constexpr auto _plus = [](type_t, value_t res, value_t args) {
        val_as(int64_t, res) =
            val_as(int64_t, val_tuple_at(args, 0)) + val_as(int64_t, val_tuple_at(args, 1));
    };

    auto const i64_t = type_get_numeric(Int64);
    type_t params[] = {i64_t, i64_t};
    TypeArray params_ar{params, sizeof(params) / sizeof(params[0])};

    auto const params_t = type_get_tuple(params_ar);

    auto const type = type_get_fn(i64_t, params_t, 0, _plus, nullptr);

    EXPECT_EQ(type->typeclass_id, Type_Fn);
    EXPECT_EQ(type->size, 0);
    EXPECT_EQ(type->alignment, 1);
    EXPECT_EQ(std_str(type_name(type)), "fn{(i64, i64), i64}");

    EXPECT_EQ(type, type_get_fn(i64_t, params_t, 0, _plus, nullptr));

    EXPECT_NE(type, type_get_fn(i64_t, params_t, 1, _plus, nullptr));

    EXPECT_EQ(type->as.fn.ret_t, i64_t);

    ASSERT_EQ(type_tuple_size(type->as.fn.args_t), 2);

    EXPECT_EQ(type_tuple_typeAt(type->as.fn.args_t, 0), i64_t);
    EXPECT_EQ(type_tuple_typeAt(type->as.fn.args_t, 1), i64_t);

    int64_t ret = 0;
    int64_t args[] = {4, 5};

    val_fn_invoke(type, value_t{&ret, i64_t}, value_t{args, params_t});

    EXPECT_EQ(ret, 9);
}

TEST_F(value, numeric) {
    auto const type = type_get_numeric(Int32);

    EXPECT_EQ(type->typeclass_id, Type_Numeric);
    EXPECT_EQ(type->size, 4);
    EXPECT_EQ(type->alignment, 4);
    EXPECT_EQ(std_str(type_name(type)), "i32");

    EXPECT_EQ(type, type_get_numeric(Int32));
    EXPECT_NE(type, type_get_numeric(Int64));

    auto const test_num = [&](ENumericValueType value_type, size_t size, char const *name) {
        auto const type = type_get_numeric(value_type);

        EXPECT_EQ(type->typeclass_id, Type_Numeric);
        EXPECT_EQ(type->size, size);
        EXPECT_EQ(type->alignment, size);

        EXPECT_EQ(std_str(type_name(type)), name);
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

TEST_F(value, tuple) {
    type_t types[] = {type_get_void(), type_get_typeref(), type_get_numeric(Int16)};
    TypeArray types_ar{types, sizeof(types) / sizeof(types[0])};

    auto const type = type_get_tuple(types_ar);

    EXPECT_EQ(type->typeclass_id, Type_Tuple);
    EXPECT_EQ(type->size, 16);
    EXPECT_EQ(type->alignment, 8);
    EXPECT_EQ(std_str(type_name(type)), "tuple{void, type, i16}");

    EXPECT_EQ(type, type_get_tuple(types_ar));

    ASSERT_EQ(type_tuple_size(type), 3);

    EXPECT_EQ(type_tuple_typeAt(type, 0), type_get_void());
    EXPECT_EQ(type_tuple_offsetAt(type, 0), 0);
    EXPECT_EQ(type_tuple_typeAt(type, 1), type_get_typeref());
    EXPECT_EQ(type_tuple_offsetAt(type, 1), 0);
    EXPECT_EQ(type_tuple_typeAt(type, 2), type_get_numeric(Int16));
    EXPECT_EQ(type_tuple_offsetAt(type, 2), 8);
}

TEST_F(value, typeref) {
    auto const type = type_get_typeref();

    EXPECT_EQ(type->typeclass_id, Type_Typeref);
    EXPECT_EQ(type->size, sizeof(size_t));
    EXPECT_EQ(type->alignment, alignof(type_t));
    EXPECT_EQ(std_str(type_name(type)), "type");

    EXPECT_EQ(type, type_get_typeref());
}

TEST_F(value, void) {
    auto const type = type_get_void();

    EXPECT_EQ(type->typeclass_id, Type_Void);
    EXPECT_EQ(type->size, 0);
    EXPECT_EQ(type->alignment, 1);
    EXPECT_EQ(std_str(type_name(type)), "void");

    EXPECT_EQ(type, type_get_void());
}
