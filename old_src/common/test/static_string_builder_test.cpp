#include "nk/str/static_string_builder.hpp"

#include <gtest/gtest.h>

#include "nk/mem/stack_allocator.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/utils.hpp"

using namespace nk;

class static_string_builder : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }

protected:
    ARRAY_SLICE(char, m_dst, 100);
    StaticStringBuilder m_builder{m_dst};
};

TEST_F(static_string_builder, empty) {
    EXPECT_EQ(std_str(m_builder.moveStr()), "");
}

TEST_F(static_string_builder, basic) {
    const std::string c_test_str = "Hello, World!";
    int n = m_builder.printf(c_test_str.c_str());
    EXPECT_EQ(n, c_test_str.size());
    auto const str = m_builder.moveStr();
    EXPECT_EQ(str.data, m_dst.data);
    EXPECT_EQ(std_str(str), c_test_str);
}

TEST_F(static_string_builder, print_big) {
    std::string test_str;
    test_str.append(50, 'a');
    int n = m_builder.printf(test_str.c_str());
    EXPECT_EQ(n, test_str.size());
    EXPECT_EQ(std_str(m_builder.moveStr()), test_str);
}

TEST_F(static_string_builder, printf) {
    int n = m_builder.printf("Hello, %s! pi=%g", "World", 3.14f);
    EXPECT_GT(n, 0);
    EXPECT_EQ(std_str(m_builder.moveStr()), "Hello, World! pi=3.14");
}

TEST_F(static_string_builder, printf_error) {
    int n = m_builder.printf("invalid format: %q");
    EXPECT_LT(n, 0);
}

TEST_F(static_string_builder, print) {
    auto const c_test_str = cs2s("test");
    int n = m_builder.print(c_test_str);
    EXPECT_EQ(n, 4);
    EXPECT_EQ(std_str(m_builder.moveStr()), "test");
}

TEST_F(static_string_builder, multiple_printfs) {
    EXPECT_EQ(m_builder.printf("one"), 3);
    EXPECT_EQ(m_builder.printf("two"), 3);
    EXPECT_EQ(m_builder.printf("three"), 5);
    EXPECT_EQ(std_str(m_builder.moveStr()), "onetwothree");
}

TEST_F(static_string_builder, allocator) {
    StackAllocator stack{};
    defer {
        stack.deinit();
    };
    m_builder.print("Hello, World!");
    auto str = m_builder.moveStr(stack);
    EXPECT_NE(str.data, m_dst.data);
    EXPECT_EQ(std_str(str), "Hello, World!");
}

TEST_F(static_string_builder, overflow) {
    ARRAY_SLICE(char, dst, 6);
    StaticStringBuilder sb{dst};
    sb << "abcdefgh";
    auto str = sb.moveStr();
    EXPECT_EQ(str.size, dst.size - 1);
    EXPECT_EQ(std_str(str), "abcde");
}
