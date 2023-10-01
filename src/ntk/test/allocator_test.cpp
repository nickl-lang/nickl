#include "ntk/allocator.h"

#include <cstring>

#include <gtest/gtest.h>

#include "ntk/logger.h"
#include "ntk/utils.h"

class allocator : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});

        m_alloc = nk_arena_getAllocator(&m_arena);
    }

    void TearDown() override {
        nk_arena_free(&m_arena);
    }

protected:
    NkArena m_arena{};
    NkAllocator m_alloc;
};

TEST_F(allocator, default) {
    auto ptr = nk_alloc_t<int>(nk_default_allocator);
    *ptr = 42;
    nk_free_t(nk_default_allocator, ptr);
}

TEST_F(allocator, basic) {
    char *ptr = nk_alloc_t<char>(m_alloc);
    ASSERT_TRUE(ptr);
    *ptr = 'a';
}

TEST_F(allocator, frames) {
    nk_alloc(m_alloc, 100);
    EXPECT_GE(m_arena.size, 100);

    auto const prev_size = m_arena.size;

    auto frame = nk_arena_grab(&m_arena);

    nk_alloc(m_alloc, 100);
    EXPECT_GE(m_arena.size, 200);

    nk_arena_popFrame(&m_arena, frame);
    EXPECT_EQ(m_arena.size, prev_size);
}

TEST_F(allocator, clear) {
    int *ptr = nk_alloc_t<int>(m_alloc, 5);
    ASSERT_TRUE(ptr);

    EXPECT_EQ((size_t)ptr & (alignof(int) - 1), 0);

    size_t const sz = sizeof(*ptr) * 5;
    std::memset(ptr, 0, sz);
    EXPECT_GE(m_arena.size, sz);

    nk_arena_popFrame(&m_arena, {0});

    EXPECT_EQ(m_arena.size, 0);
}

TEST_F(allocator, realloc) {
    EXPECT_EQ(m_arena.size, 0);

    auto ptr1 = nk_alloc_t<int>(m_alloc);
    EXPECT_GE(m_arena.size, sizeof(int));
    *ptr1 = 42;

    auto ptr2 = nk_realloc_t(m_alloc, 2, ptr1, 1);
    EXPECT_GE(m_arena.size, 2 * sizeof(int));

    EXPECT_EQ(ptr1, ptr2);

    EXPECT_EQ(ptr1[0], 42);

    EXPECT_EQ(ptr2[0], 42);
    EXPECT_EQ(ptr2[1], 0);
}

TEST_F(allocator, realloc_memcpy) {
    EXPECT_EQ(m_arena.size, 0);

    auto ptr1 = nk_alloc_t<int>(m_alloc);
    EXPECT_GE(m_arena.size, sizeof(int));
    *ptr1 = 42;

    nk_arena_alloc(&m_arena, 1);

    auto ptr2 = nk_realloc_t(m_alloc, 2, ptr1, 1);
    EXPECT_GE(m_arena.size, 2 * sizeof(int));

    EXPECT_NE(ptr1, ptr2);

    EXPECT_EQ(ptr1[0], 42);

    EXPECT_EQ(ptr2[0], 42);
    EXPECT_EQ(ptr2[1], 0);
}

TEST_F(allocator, align) {
    EXPECT_EQ(m_arena.size, 0);
    defer {
        EXPECT_EQ(m_arena.size, 0);
    };

    auto const frame = nk_arena_grab(&m_arena);
    defer {
        nk_arena_popFrame(&m_arena, frame);
    };

    nk_arena_allocAligned(&m_arena, 1, 1);
    EXPECT_GE(m_arena.size, 1);

    void *ptr = nk_arena_allocAligned(&m_arena, 8, 8);
    EXPECT_EQ((size_t)ptr & 7, 0);
    EXPECT_GE(m_arena.size, 16);
}

TEST_F(allocator, free_noop) {
    static constexpr auto c_size = 2;

    auto ptr1 = nk_alloc_t<int64_t>(m_alloc, c_size);
    ptr1[0] = 42;
    ptr1[1] = 43;

    auto ptr2 = nk_alloc_t<int64_t>(m_alloc, c_size);

    EXPECT_EQ(ptr1[0], 42);
    EXPECT_EQ(ptr1[1], 43);

    nk_free_t(m_alloc, ptr1, c_size);

    EXPECT_EQ(ptr1[0], 42);
    EXPECT_EQ(ptr1[1], 43);

    EXPECT_EQ(ptr2[0], 0);
    EXPECT_EQ(ptr2[1], 0);
}
