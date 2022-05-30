#include "nk/common/string_builder.hpp"

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"

class string_builder : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
    }

    void TearDown() override {
        m_arena.deinit();
    }

protected:
    StringBuilder m_builder{};
    ArenaAllocator m_arena;
};

TEST_F(string_builder, empty) {
    EXPECT_EQ(std_str(m_builder.moveStr(m_arena)), "");
}

TEST_F(string_builder, basic) {
    const std::string c_test_str = "Hello, World!";
    int n = m_builder.printf(c_test_str.c_str());
    EXPECT_EQ(n, c_test_str.size());
    EXPECT_EQ(std_str(m_builder.moveStr(m_arena)), c_test_str);
}

TEST_F(string_builder, reserve) {
    const std::string c_test_str = "Hello, World!";
    m_builder.reserve(c_test_str.size());
    int n = m_builder.printf(c_test_str.c_str());
    EXPECT_EQ(n, c_test_str.size());
    EXPECT_EQ(std_str(m_builder.moveStr(m_arena)), c_test_str);
}

TEST_F(string_builder, reserve_half) {
    const std::string c_test_str = "Helloooooooooooooooooooooooooooooooooooooooooooooooooo, World!";
    m_builder.reserve(c_test_str.size() / 2);
    int n = m_builder.printf(c_test_str.c_str());
    EXPECT_EQ(n, c_test_str.size());
    EXPECT_EQ(std_str(m_builder.moveStr(m_arena)), c_test_str);
}

TEST_F(string_builder, print_big) {
    std::string test_str;
    test_str.append(5000, 'a');
    int n = m_builder.printf(test_str.c_str());
    EXPECT_EQ(n, test_str.size());
    EXPECT_EQ(std_str(m_builder.moveStr(m_arena)), test_str);
}

TEST_F(string_builder, printf) {
    int n = m_builder.printf("Hello, %s! pi=%g", "World", 3.14f);
    EXPECT_GT(n, 0);
    EXPECT_EQ(std_str(m_builder.moveStr(m_arena)), "Hello, World! pi=3.14");
}

TEST_F(string_builder, printf_error) {
    int n = m_builder.printf("invalid format: %q");
    EXPECT_LT(n, 0);
}
