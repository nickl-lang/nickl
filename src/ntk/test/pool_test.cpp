#include "ntk/pool.h"

#include <gtest/gtest.h>

#include "ntk/arena.h"
#include "ntk/log.h"

class Pool : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});
    }

    void TearDown() override {
    }
};

NK_POOL_DEFINE(IntPool, int);

TEST_F(Pool, basic) {
    IntPool pool{};
    defer {
        IntPool_free(&pool);
    };

    EXPECT_EQ(pool.items.size, 0);

    int *obj = IntPool_alloc(&pool);
    *obj = 42;
    EXPECT_EQ(pool.items.size, 1);

    IntPool_release(&pool, obj);
    EXPECT_EQ(pool.items.size, 1);

    ASSERT_TRUE(pool.next);
    EXPECT_EQ(&pool.next->item, obj);
}

TEST_F(Pool, reuse) {
    NkArena arena{};
    defer {
        nk_arena_free(&arena);
    };

    IntPool pool{NK_POOL_INIT(nk_arena_getAllocator(&arena))};

    int *a = IntPool_alloc(&pool);
    EXPECT_EQ(*a, 0);
    IntPool_release(&pool, a);

    int *b = IntPool_alloc(&pool);
    EXPECT_EQ(*b, 0);
    EXPECT_EQ(a, b);

    int *c = IntPool_alloc(&pool);
    EXPECT_EQ(*c, 0);
    EXPECT_NE(a, c);
}
