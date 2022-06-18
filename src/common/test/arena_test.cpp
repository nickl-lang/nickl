#include "nk/common/arena.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/string.hpp"

class arena : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
        m_arena.deinit();
    }

protected:
    Arena<uint8_t> m_arena{};
};

TEST_F(arena, push) {
    auto const c_test_str = cs2s("hello world");

    char *const mem = (char *)m_arena.push(c_test_str.size).begin();
    Slice<char> const str{mem, c_test_str.size};
    c_test_str.copy(str);

    EXPECT_EQ(m_arena.size, c_test_str.size);
    EXPECT_EQ(std_str(c_test_str), std_str(str));
}

TEST_F(arena, pop) {
    m_arena.push(42);
    EXPECT_EQ(m_arena.size, 42);

    m_arena.pop(2);
    EXPECT_EQ(m_arena.size, 40);

    m_arena.pop(21);
    EXPECT_EQ(m_arena.size, 19);

    m_arena.pop(19);
    EXPECT_EQ(m_arena.size, 0);
}

TEST_F(arena, clear) {
    m_arena.push(42);
    EXPECT_EQ(m_arena.size, 42);

    m_arena.clear();
    EXPECT_EQ(m_arena.size, 0);
}

TEST_F(arena, copy_empty) {
    m_arena.copy(nullptr);
}

TEST_F(arena, copy_empty_after_clear) {
    m_arena.push();
    m_arena.clear();

    m_arena.copy(nullptr);
}

TEST_F(arena, pop_multiple_blocks) {
    static constexpr size_t c_test_size = 1024;

    for (size_t i = 0; i < c_test_size; i++) {
        *m_arena.push() = 0;
    }
    EXPECT_EQ(m_arena.size, c_test_size);

    m_arena.pop(c_test_size);
    EXPECT_EQ(m_arena.size, 0);

    auto mem = m_arena.push(c_test_size);
    EXPECT_EQ(m_arena.size, c_test_size);
    std::memset(mem.data, 0, mem.size);
}
