#include "nk/common/allocator.hpp"

#include <cstring>

#include <gtest/gtest.h>

#include "nk/common/logger.h"

class allocator : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});

        m_alloc = nk_arena_getAllocator(&m_arena);
    }

    void TearDown() override {
        nk_arena_free(&m_arena);
    }

protected:
    NkArenaAllocator m_arena{};
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
    EXPECT_EQ(m_arena.size, 100);

    auto frame = nk_arena_grab(&m_arena);

    nk_alloc(m_alloc, 100);
    EXPECT_EQ(m_arena.size, 200);

    nk_arena_popFrame(&m_arena, frame);
    EXPECT_EQ(m_arena.size, 100);
}

TEST_F(allocator, align) {
    struct A {
        uint8_t a;
        std::max_align_t b;
    };

    auto ptr = nk_alloc_t<A>(m_alloc);
    EXPECT_EQ((size_t)ptr % alignof(A), 0);
}

TEST_F(allocator, clear) {
    int *ptr = nk_alloc_t<int>(m_alloc, 5);
    ASSERT_TRUE(ptr);

    size_t const sz = sizeof(*ptr) * 5;
    std::memset(ptr, 0, sz);

    EXPECT_EQ(m_arena.size, sz);

    nk_arena_popFrame(&m_arena, {0});

    EXPECT_EQ(m_arena.size, 0);
}
