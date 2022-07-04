#include "nk/str/dynamic_string_builder.hpp"

#include <gtest/gtest.h>

#include "nk/mem/stack_allocator.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/utils.hpp"

using namespace nk;

class string_builder : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }

protected:
    DynamicStringBuilder m_builder{};
    ARRAY_SLICE(char, m_dst, 10000);
};

TEST_F(string_builder, empty) {
    EXPECT_EQ(std_str(m_builder.moveStr(m_dst)), "");
}

TEST_F(string_builder, basic) {
    const std::string c_test_str = "Hello, World!";
    int n = m_builder.printf(c_test_str.c_str());
    EXPECT_EQ(n, c_test_str.size());
    EXPECT_EQ(std_str(m_builder.moveStr(m_dst)), c_test_str);
}

TEST_F(string_builder, reserve) {
    const std::string c_test_str = "Hello, World!";
    m_builder.reserve(c_test_str.size());
    int n = m_builder.printf(c_test_str.c_str());
    EXPECT_EQ(n, c_test_str.size());
    EXPECT_EQ(std_str(m_builder.moveStr(m_dst)), c_test_str);
}

TEST_F(string_builder, reserve_half) {
    const std::string c_test_str = "Helloooooooooooooooooooooooooooooooooooooooooooooooooo, World!";
    m_builder.reserve(c_test_str.size() / 2);
    int n = m_builder.printf(c_test_str.c_str());
    EXPECT_EQ(n, c_test_str.size());
    EXPECT_EQ(std_str(m_builder.moveStr(m_dst)), c_test_str);
}

TEST_F(string_builder, print_big) {
    std::string test_str;
    test_str.append(5000, 'a');
    int n = m_builder.printf(test_str.c_str());
    EXPECT_EQ(n, test_str.size());
    EXPECT_EQ(std_str(m_builder.moveStr(m_dst)), test_str);
}

TEST_F(string_builder, printf) {
    int n = m_builder.printf("Hello, %s! pi=%g", "World", 3.14f);
    EXPECT_GT(n, 0);
    EXPECT_EQ(std_str(m_builder.moveStr(m_dst)), "Hello, World! pi=3.14");
}

TEST_F(string_builder, printf_error) {
    int n = m_builder.printf("invalid format: %q");
    EXPECT_LT(n, 0);
}

TEST_F(string_builder, print) {
    auto const c_test_str = cs2s("test");
    int n = m_builder.print(c_test_str);
    EXPECT_EQ(n, 4);
    EXPECT_EQ(std_str(m_builder.moveStr(m_dst)), "test");
}

TEST_F(string_builder, multiple_printfs) {
    EXPECT_EQ(m_builder.printf("one"), 3);
    EXPECT_EQ(m_builder.printf("two"), 3);
    EXPECT_EQ(m_builder.printf("three"), 5);
    EXPECT_EQ(std_str(m_builder.moveStr(m_dst)), "onetwothree");
}

TEST_F(string_builder, allocator) {
    StackAllocator stack{};
    defer {
        stack.deinit();
    };
    m_builder.print("Hello, World!");
    auto str = m_builder.moveStr(stack);
    EXPECT_EQ(std_str(str), "Hello, World!");
}
