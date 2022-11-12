#include "nk/mem/static_allocator.hpp"

#include <cstring>

#include <gtest/gtest.h>

#include "nk/utils/logger.h"

using namespace nk;

class static_allocator : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }

protected:
    ARRAY_SLICE(uint8_t, m_dst, 100);
    StaticAllocator m_allocator{m_dst};
};

TEST_F(static_allocator, basic) {
    char *ptr = m_allocator.alloc<char>();
    ASSERT_TRUE(ptr);
    *ptr = 'a';

    EXPECT_EQ(m_dst.data, (uint8_t *)ptr);
}

TEST_F(static_allocator, multiple_allocs) {
    auto a = m_allocator.alloc<int>();
    auto b = m_allocator.alloc<int>();
    auto c = m_allocator.alloc<int>();

    ASSERT_TRUE(a);
    *a = 1;
    ASSERT_TRUE(b);
    *b = 2;
    ASSERT_TRUE(c);
    *c = 3;

    EXPECT_EQ(a, (int *)m_dst.data);
    EXPECT_EQ(b, (int *)m_dst.data + 1);
    EXPECT_EQ(c, (int *)m_dst.data + 2);
}

TEST_F(static_allocator, align) {
    struct A {
        uint8_t a;
        std::max_align_t b;
    };

    auto ptr = m_allocator.alloc<A>();
    ASSERT_NE(ptr, nullptr);
    *ptr = A{};

    EXPECT_EQ((size_t)ptr.data % alignof(A), 0);
}

TEST_F(static_allocator, clear) {
    int *ptr = m_allocator.alloc<int>(5);
    ASSERT_TRUE(ptr);

    size_t const sz = sizeof(*ptr) * 5;
    std::memset(ptr, 0, sz);

    EXPECT_EQ(m_allocator.size(), sz);

    m_allocator.clear();

    EXPECT_EQ(m_allocator.size(), 0);
}

TEST_F(static_allocator, alloc_full) {
    char *ptr = m_allocator.alloc<char>(m_dst.size);
    ASSERT_NE(ptr, nullptr);
    std::memset(ptr, 0, m_dst.size);
    EXPECT_EQ(m_allocator.size(), m_dst.size);
}

TEST_F(static_allocator, frames) {
    m_allocator.alloc<uint8_t>(12);
    EXPECT_EQ(m_allocator.size(), 12);

    auto frame = m_allocator.pushFrame();

    m_allocator.alloc<uint8_t>(34);
    EXPECT_EQ(m_allocator.size(), 46);

    m_allocator.popFrame(frame);
    EXPECT_EQ(m_allocator.size(), 12);
}
