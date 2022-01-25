#include "nk/vm/value.hpp"

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"

using namespace nk::vm;

namespace {

class value : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
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
    EXPECT_EQ(type_name(&m_arena, type).view(), std::string_view{"Array(i32, 42)"});

    EXPECT_EQ(type, type_get_array(i32_t, c_array_size));

    EXPECT_EQ(type->as.arr.elem_type, i32_t);
    EXPECT_EQ(type->as.arr.elem_count, c_array_size);
}

TEST_F(value, ptr) {
    auto const void_t = type_get_void();
    auto const type = type_get_ptr(void_t);

    EXPECT_EQ(type->typeclass_id, Type_Ptr);
    EXPECT_EQ(type->size, sizeof(void *));
    EXPECT_EQ(type->alignment, alignof(void *));
    EXPECT_EQ(type_name(&m_arena, type).view(), std::string_view{"Ptr(void)"});

    EXPECT_EQ(type, type_get_ptr(void_t));

    auto const type_t = type_get_typeref();
    EXPECT_NE(type, type_get_ptr(type_t));

    EXPECT_EQ(type->as.ptr.target_type, void_t);
}

static void _plus(value_t res, value_t a, value_t b) {
    val_as(int64_t, res) = val_as(int64_t, a) + val_as(int64_t, b);
}

TEST_F(value, fn) {
    auto const i64_t = type_get_numeric(Int64);
    type_t params[] = {i64_t, i64_t};
    TypeArray params_ar{params, sizeof(params) / sizeof(void *)};

    auto const params_t = type_get_tuple(&m_arena, params_ar);

    auto const type = type_get_fn(i64_t, params_t, 0, (void *)_plus, nullptr);

    EXPECT_EQ(type->typeclass_id, Type_Fn);
    EXPECT_EQ(type->size, 0);
    EXPECT_EQ(type->alignment, 1);
    EXPECT_EQ(type_name(&m_arena, type).view(), std::string_view{"Fn((i64, i64), i64)"});

    EXPECT_EQ(type, type_get_fn(i64_t, params_t, 0, (void *)_plus, nullptr));

    EXPECT_NE(type, type_get_fn(i64_t, params_t, 1, (void *)_plus, nullptr));

    EXPECT_EQ(type->as.fn.ret_t, i64_t);

    ASSERT_EQ(type->as.fn.args_t->as.tuple.elems.size, 2);

    EXPECT_EQ(type->as.fn.args_t->as.tuple.elems.data[0].type, i64_t);
    EXPECT_EQ(type->as.fn.args_t->as.tuple.elems.data[1].type, i64_t);
}

TEST_F(value, numeric) {
    auto const type = type_get_numeric(Int32);

    EXPECT_EQ(type->typeclass_id, Type_Numeric);
    EXPECT_EQ(type->size, 4);
    EXPECT_EQ(type->alignment, 4);
    EXPECT_EQ(type_name(&m_arena, type).view(), std::string_view{"i32"});

    EXPECT_EQ(type, type_get_numeric(Int32));
    EXPECT_NE(type, type_get_numeric(Int64));

    auto const test_num = [&](ENumericValueType value_type, size_t size, char const *name) {
        auto const type = type_get_numeric(value_type);

        EXPECT_EQ(type->typeclass_id, Type_Numeric);
        EXPECT_EQ(type->size, size);
        EXPECT_EQ(type->alignment, size);

        EXPECT_EQ(type_name(&m_arena, type).view(), std::string_view{name});
    };

    test_num(Int0, 0, "i0");
    test_num(Int1, 1, "i1");
    test_num(Int8, 1, "i8");
    test_num(Int16, 2, "i16");
    test_num(Int32, 4, "i32");
    test_num(Int64, 8, "i64");
    test_num(Uint0, 0, "u0");
    test_num(Uint1, 1, "u1");
    test_num(Uint8, 1, "u8");
    test_num(Uint16, 2, "u16");
    test_num(Uint32, 4, "u32");
    test_num(Uint64, 8, "u64");
    test_num(Float32, 4, "f32");
    test_num(Float64, 8, "f64");
}

TEST_F(value, tuple) {
    type_t types[] = {type_get_void(), type_get_typeref(), type_get_numeric(Int16)};
    TypeArray types_ar{types, sizeof(types) / sizeof(void *)};

    auto const type = type_get_tuple(&m_arena, types_ar);

    EXPECT_EQ(type->typeclass_id, Type_Tuple);
    EXPECT_EQ(type->size, 16);
    EXPECT_EQ(type->alignment, 8);
    EXPECT_EQ(type_name(&m_arena, type).view(), std::string_view{"Tuple(void, type, i16)"});

    EXPECT_EQ(type, type_get_tuple(&m_arena, types_ar));

    ASSERT_EQ(type->as.tuple.elems.size, 3);

    EXPECT_EQ(type->as.tuple.elems.data[0].type, type_get_void());
    EXPECT_EQ(type->as.tuple.elems.data[0].offset, 0);
    EXPECT_EQ(type->as.tuple.elems.data[1].type, type_get_typeref());
    EXPECT_EQ(type->as.tuple.elems.data[1].offset, 0);
    EXPECT_EQ(type->as.tuple.elems.data[2].type, type_get_numeric(Int16));
    EXPECT_EQ(type->as.tuple.elems.data[2].offset, 8);
}

TEST_F(value, typeref) {
    auto const type = type_get_typeref();

    EXPECT_EQ(type->typeclass_id, Type_Typeref);
    EXPECT_EQ(type->size, sizeof(size_t));
    EXPECT_EQ(type->alignment, alignof(type_t));
    EXPECT_EQ(type_name(&m_arena, type).view(), std::string_view{"type"});

    EXPECT_EQ(type, type_get_typeref());
}

TEST_F(value, void) {
    auto const type = type_get_void();

    EXPECT_EQ(type->typeclass_id, Type_Void);
    EXPECT_EQ(type->size, 0);
    EXPECT_EQ(type->alignment, 1);
    EXPECT_EQ(type_name(&m_arena, type).view(), std::string_view{"void"});

    EXPECT_EQ(type, type_get_void());
}
