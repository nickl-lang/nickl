#include "nk/common/stack_allocator.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.h"

class stack_allocator : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
        m_allocator.deinit();
    }

protected:
    StackAllocator m_allocator{};
};

TEST_F(stack_allocator, basic) {
    char *ptr = m_allocator.alloc<char>();
    ASSERT_TRUE(ptr);
    *ptr = 'a';
}

/// @TODO Add more StackAllocator tests
