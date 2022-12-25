#define private public
#include "nk/mem/stack_allocator.hpp"
#undef private

#include <cstring>

#include <gtest/gtest.h>

#include "nk/utils/logger.h"

using namespace nk;

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
    EXPECT_EQ(m_allocator.size(), 100);

    auto frame = m_allocator.pushFrame();

    m_allocator.alloc<uint8_t>(100);
    EXPECT_EQ(m_allocator.size(), 200);

    m_allocator.popFrame(frame);
    EXPECT_EQ(m_allocator.size(), 100);
}

TEST_F(stack_allocator, align) {
    struct A {
        uint8_t a;
        std::max_align_t b;
    };

    auto ptr = m_allocator.alloc<A>();
    EXPECT_EQ((size_t)ptr.data % alignof(A), 0);
}

TEST_F(stack_allocator, clear) {
    int *ptr = m_allocator.alloc<int>(5);
    ASSERT_TRUE(ptr);

    size_t const sz = sizeof(*ptr) * 5;
    std::memset(ptr, 0, sz);

    EXPECT_EQ(m_allocator.size(), sz);

    m_allocator.clear();

    EXPECT_EQ(m_allocator.size(), 0);
}