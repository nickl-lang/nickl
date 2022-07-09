#include "nkl/lang/value.hpp"

#include <gtest/gtest.h>

#include "nk/mem/stack_allocator.hpp"
#include "nk/str/dynamic_string_builder.hpp"
#include "nk/utils/logger.h"

namespace {

using namespace nk;
using namespace nkl;

LOG_USE_SCOPE(nkl::value::test);

class value : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
        m_arena.deinit();
        m_sb.deinit();
    }

protected:
    DynamicStringBuilder m_sb{};
    StackAllocator m_arena{};
};

} // namespace

TEST_F(value, struct_type) {
    auto f64_t = type_get_numeric(vm::Float64);
    ARRAY_SLICE_INIT(Field, fields, Field{cs2id("x"), f64_t, {}}, Field{cs2id("y"), f64_t, {}});

    auto type = type_get_struct(fields);

    EXPECT_EQ(type->typeclass_id, Type_Struct);
    EXPECT_EQ(type->size, 16);
    EXPECT_EQ(type->alignment, 8);
    EXPECT_EQ(std_str(type_name(type, m_sb).moveStr(m_arena)), "struct{x: f64, y: f64}");

    EXPECT_EQ(type->id, type_get_struct(fields)->id);

    ASSERT_EQ(type_tuple_size(type), 2);

    EXPECT_EQ(type_tuple_typeAt(type, 0), f64_t);
    EXPECT_EQ(type_tuple_offsetAt(type, 0), 0);
    EXPECT_EQ(type_tuple_typeAt(type, 1), f64_t);
    EXPECT_EQ(type_tuple_offsetAt(type, 1), 8);

    ASSERT_EQ(type_struct_size(type), 2);

    EXPECT_EQ(type_struct_typeAt(type, 0), f64_t);
    EXPECT_EQ(std_str(id2s(type_struct_nameAt(type, 0))), "x");
    EXPECT_EQ(type_struct_typeAt(type, 1), f64_t);
    EXPECT_EQ(std_str(id2s(type_struct_nameAt(type, 1))), "y");
}
