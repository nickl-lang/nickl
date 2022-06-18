#include "nk/common/arena.hpp"

#include <cstring>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/string.hpp"

class arena : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
        Arena_deinit(&m_allocator);
    }

protected:
    Arena m_allocator{};
};

TEST_F(arena, push) {
    auto const c_test_str = cs2s("hello world");

    char *const mem = (char *)Arena_push(&m_allocator, c_test_str.size);
    Slice<char> const str{mem, c_test_str.size};
    c_test_str.copy(str);

    EXPECT_EQ(m_allocator.size, c_test_str.size);
    EXPECT_EQ(std_str(c_test_str), std_str(str));
}

TEST_F(arena, pop) {
    Arena_push(&m_allocator, 42);
    EXPECT_EQ(m_allocator.size, 42);

    Arena_pop(&m_allocator, 2);
    EXPECT_EQ(m_allocator.size, 40);

    Arena_pop(&m_allocator, 21);
    EXPECT_EQ(m_allocator.size, 19);

    Arena_pop(&m_allocator, 19);
    EXPECT_EQ(m_allocator.size, 0);
}

TEST_F(arena, clear) {
    Arena_push(&m_allocator, 42);
    EXPECT_EQ(m_allocator.size, 42);

    Arena_clear(&m_allocator);
    EXPECT_EQ(m_allocator.size, 0);
}

TEST_F(arena, copy_empty) {
    Arena_copy(&m_allocator, nullptr);
}

TEST_F(arena, copy_empty_after_clear) {
    Arena_push(&m_allocator, 1);
    Arena_clear(&m_allocator);

    Arena_copy(&m_allocator, nullptr);
}

TEST_F(arena, pop_multiple_blocks) {
    Arena_reserve(&m_allocator, 1);

    for (size_t i = 0; i < 1024; i++) {
        Arena_push(&m_allocator, 1);
    }
    EXPECT_EQ(m_allocator.size, 1024);

    Arena_pop(&m_allocator, 1024);
    EXPECT_EQ(m_allocator.size, 0);

    Arena_push(&m_allocator, 1024);
    EXPECT_EQ(m_allocator.size, 1024);
}
