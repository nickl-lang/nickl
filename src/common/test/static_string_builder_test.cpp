#include "nk/common/static_string_builder.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/stack_allocator.hpp"
#include "nk/common/utils.hpp"

using namespace nk;

class static_string_builder : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }

protected:
    ARRAY_SLICE(char, m_dst, 10000);
    StaticStringBuilder m_builder{m_dst};
};

TEST_F(static_string_builder, empty) {
    EXPECT_EQ(std_str(m_builder.moveStr()), "");
}

TEST_F(static_string_builder, basic) {
    const std::string c_test_str = "Hello, World!";
    int n = m_builder.printf(c_test_str.c_str());
    EXPECT_EQ(n, c_test_str.size());
    EXPECT_EQ(std_str(m_builder.moveStr()), c_test_str);
}

TEST_F(static_string_builder, print_big) {
    std::string test_str;
    test_str.append(5000, 'a');
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
    auto str = m_builder.moveStr();
    EXPECT_EQ(std_str(str), "Hello, World!");
}
