#include "nk/common/stack_allocator.h"

#include <cstring>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/string.hpp"

class stack_allocator : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
        StackAllocator_deinit(&m_allocator);
    }

protected:
    StackAllocator m_allocator{};
};

TEST_F(stack_allocator, push) {
    auto const c_test_str = cs2s("hello world");

    char *const mem = (char *)StackAllocator_push(&m_allocator, c_test_str.size);
    Slice<char> const str{mem, c_test_str.size};
    c_test_str.copy(str);

    EXPECT_EQ(m_allocator.size, c_test_str.size);
    EXPECT_EQ(std_str(c_test_str), std_str(str));
}

TEST_F(stack_allocator, pop) {
    StackAllocator_push(&m_allocator, 42);
    EXPECT_EQ(m_allocator.size, 42);

    StackAllocator_pop(&m_allocator, 2);
    EXPECT_EQ(m_allocator.size, 40);

    StackAllocator_pop(&m_allocator, 21);
    EXPECT_EQ(m_allocator.size, 19);

    StackAllocator_pop(&m_allocator, 19);
    EXPECT_EQ(m_allocator.size, 0);
}

TEST_F(stack_allocator, clear) {
    StackAllocator_push(&m_allocator, 42);
    EXPECT_EQ(m_allocator.size, 42);

    StackAllocator_clear(&m_allocator);
    EXPECT_EQ(m_allocator.size, 0);
}

TEST_F(stack_allocator, copy_empty) {
    StackAllocator_copy(&m_allocator, nullptr);
}

TEST_F(stack_allocator, copy_empty_after_clear) {
    StackAllocator_push(&m_allocator, 1);
    StackAllocator_clear(&m_allocator);

    StackAllocator_copy(&m_allocator, nullptr);
}

TEST_F(stack_allocator, pop_multiple_blocks) {
    StackAllocator_reserve(&m_allocator, 1);

    for (size_t i = 0; i < 1024; i++) {
        StackAllocator_push(&m_allocator, 1);
    }
    EXPECT_EQ(m_allocator.size, 1024);

    StackAllocator_pop(&m_allocator, 1024);
    EXPECT_EQ(m_allocator.size, 0);

    StackAllocator_push(&m_allocator, 1024);
    EXPECT_EQ(m_allocator.size, 1024);
}
