#define private public
#include "nk/mem/static_allocator.hpp"
#undef private

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
    uint8_t m_buf[100];
    Slice<uint8_t> m_dst{m_buf, sizeof(m_buf)};
    StaticAllocator m_allocator{m_dst};
};

TEST_F(static_allocator, basic) {
    char *ptr = m_allocator.alloc<char>();
    ASSERT_TRUE(ptr);
    *ptr = 'a';

    EXPECT_EQ(m_buf, (uint8_t *)ptr);
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

    EXPECT_EQ(a, (int *)m_buf);
    EXPECT_EQ(b, (int *)m_buf + 1);
    EXPECT_EQ(c, (int *)m_buf + 2);
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

    EXPECT_EQ(m_allocator.m_size, sz);

    m_allocator.clear();

    EXPECT_EQ(m_allocator.m_size, 0);
}

TEST_F(static_allocator, alloc_full) {
    char *ptr = m_allocator.alloc<char>(sizeof(m_buf));
    ASSERT_NE(ptr, nullptr);
    std::memset(ptr, 0, sizeof(m_buf));
    EXPECT_EQ(m_allocator.m_size, sizeof(m_buf));
}
