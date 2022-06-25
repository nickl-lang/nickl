#define private public
#include "nk/common/stack_allocator.hpp"
#undef private

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

TEST_F(stack_allocator, reserve) {
    EXPECT_EQ(m_allocator.m_arena._first_block, nullptr);
    m_allocator.reserve(1000);
    EXPECT_NE(m_allocator.m_arena._first_block, nullptr);
}

TEST_F(stack_allocator, frames) {
    m_allocator.alloc<uint8_t>(100);
    EXPECT_EQ(m_allocator.m_arena.size, 100);

    auto frame = m_allocator.pushFrame();

    m_allocator.alloc<uint8_t>(100);
    EXPECT_EQ(m_allocator.m_arena.size, 200);

    m_allocator.popFrame(frame);
    EXPECT_EQ(m_allocator.m_arena.size, 100);
}

TEST_F(stack_allocator, align) {
    struct A {
        uint8_t a;
        std::max_align_t b;
    };

    auto ptr = m_allocator.alloc<A>();
    EXPECT_EQ((size_t)ptr % alignof(A), 0);
}
