#include "ntk/allocator.h"

#include <cstring>

#include <gtest/gtest.h>

#include "ntk/arena.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/time.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(test);

class allocator : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});

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
    auto ptr = nk_allocT<int>(nk_default_allocator);
    *ptr = 42;
    nk_freeT(nk_default_allocator, ptr);
}

TEST_F(allocator, basic) {
    char *ptr = nk_allocT<char>(m_alloc);
    ASSERT_TRUE(ptr);
    *ptr = 'a';
}

TEST_F(allocator, frames) {
    nk_alloc(m_alloc, 100);
    EXPECT_GE(m_arena.size, 100u);

    auto const prev_size = m_arena.size;

    auto frame = nk_arena_grab(&m_arena);

    nk_alloc(m_alloc, 100);
    EXPECT_GE(m_arena.size, 200u);

    nk_arena_popFrame(&m_arena, frame);
    EXPECT_EQ(m_arena.size, prev_size);
}

TEST_F(allocator, clear) {
    int *ptr = nk_allocT<int>(m_alloc, 5);
    ASSERT_TRUE(ptr);

    EXPECT_EQ((usize)ptr & (alignof(int) - 1), 0u);

    usize const sz = sizeof(*ptr) * 5;
    std::memset(ptr, 0, sz);
    EXPECT_GE(m_arena.size, sz);

    nk_arena_popFrame(&m_arena, {0});

    EXPECT_EQ(m_arena.size, 0u);
}

TEST_F(allocator, realloc) {
    EXPECT_EQ(m_arena.size, 0u);

    auto ptr1 = nk_allocT<int>(m_alloc);
    EXPECT_GE(m_arena.size, sizeof(int));
    *ptr1 = 42;

    auto ptr2 = nk_reallocT(m_alloc, 2, ptr1, 1);
    EXPECT_GE(m_arena.size, 2 * sizeof(int));

    EXPECT_EQ(ptr1, ptr2);

    EXPECT_EQ(ptr1[0], 42);

    EXPECT_EQ(ptr2[0], 42);
    EXPECT_EQ(ptr2[1], 0);
}

TEST_F(allocator, realloc_memcpy) {
    EXPECT_EQ(m_arena.size, 0u);

    auto ptr1 = nk_allocT<int>(m_alloc);
    EXPECT_GE(m_arena.size, sizeof(int));
    *ptr1 = 42;

    nk_arena_alloc(&m_arena, 1);

    auto ptr2 = nk_reallocT(m_alloc, 2, ptr1, 1);
    EXPECT_GE(m_arena.size, 2 * sizeof(int));

    EXPECT_NE(ptr1, ptr2);

    EXPECT_EQ(ptr1[0], 42);

    EXPECT_EQ(ptr2[0], 42);
    EXPECT_EQ(ptr2[1], 0);
}

TEST_F(allocator, align) {
    EXPECT_EQ(m_arena.size, 0u);
    defer {
        EXPECT_EQ(m_arena.size, 0u);
    };

    auto const frame = nk_arena_grab(&m_arena);
    defer {
        nk_arena_popFrame(&m_arena, frame);
    };

    nk_arena_allocAligned(&m_arena, 1, 1);
    EXPECT_GE(m_arena.size, 1u);

    void *ptr = nk_arena_allocAligned(&m_arena, 8, 8);
    EXPECT_EQ((usize)ptr & 7, 0u);
    EXPECT_GE(m_arena.size, 16u);
}

TEST_F(allocator, free_noop) {
    static constexpr auto c_size = 2;

    auto ptr1 = nk_allocT<i64>(m_alloc, c_size);
    ptr1[0] = 42;
    ptr1[1] = 43;

    auto ptr2 = nk_allocT<i64>(m_alloc, c_size);

    EXPECT_EQ(ptr1[0], 42);
    EXPECT_EQ(ptr1[1], 43);

    nk_freeT(m_alloc, ptr1, c_size);

    EXPECT_EQ(ptr1[0], 42);
    EXPECT_EQ(ptr1[1], 43);

    EXPECT_EQ(ptr2[0], 0);
    EXPECT_EQ(ptr2[1], 0);
}

TEST_F(allocator, arena_stress) {
    NkDynArray(NkArenaFrame) frames{};
    usize total_size = 0;
    NkArena arena{};
    defer {
        nk_arena_free(&arena);
        nkda_free(&frames);
    };

    srand(nk_now_ns());

    static constexpr auto N = 1000000;
    static constexpr auto MaxAllocSize = 256;
    static constexpr auto MaxAllocCount = 15;

    for (usize i = 0; i < N; i++) {
        if (rand() % 2 && frames.size) {
            auto const frame = nk_slice_last(frames);

            auto const sz_delta = arena.size - frame.size;
            total_size -= sz_delta;
            NK_LOG_INF("Popping %zu bytes, total_size=%zu", sz_delta, total_size);

            nk_arena_popFrame(&arena, frame);
            nkda_pop(&frames, 1);
        } else if (frames.size < MaxAllocCount) {
            auto const frame = nk_arena_grab(&arena);
            nkda_append(&frames, frame);

            usize const sz = rand() % MaxAllocSize + 1;

            u8 align[] = {1, 2, 4, 8, 16};
            void *data = nk_arena_allocAligned(&arena, sz, align[rand() % 5]);
            for (usize i = 0; i < sz; i++) {
                *((char *)data + i) = 'a';
            }

            auto const sz_delta = arena.size - frame.size;
            total_size += sz_delta;
            NK_LOG_INF("Pushing %zu bytes, total_size=%zu", sz_delta, total_size);
        }
    }
}
