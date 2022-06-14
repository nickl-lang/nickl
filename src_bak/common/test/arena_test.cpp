#include "nk/common/arena.hpp"

#include <gtest/gtest.h>

#include "nk/common/utils.hpp"

class arena : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(arena, basic) {
    auto ar = ArenaAllocator::create(1);
    defer {
        ar.deinit();
    };

    struct ValueType {
        uint64_t a;
        uint64_t b;
        uint64_t c;
    };

    size_t prev_size = 0;
    EXPECT_EQ(ar._seq.size, 0);

    prev_size = ar._seq.size;
    ar.alloc<ValueType>();
    EXPECT_GT(ar._seq.size, prev_size);

    prev_size = ar._seq.size;
    ar.alloc<ValueType>();
    EXPECT_GT(ar._seq.size, prev_size);
}

TEST_F(arena, clear) {
    auto ar = ArenaAllocator::create(1);
    defer {
        ar.deinit();
    };

    ar.alloc<int>();
    EXPECT_NE(ar._seq.size, 0);

    ar.clear();
    EXPECT_EQ(ar._seq.size, 0);
}

TEST_F(arena, aligned) {
    auto ar = ArenaAllocator::create(100);
    defer {
        ar.deinit();
    };

    ar.alloc<uint8_t>();

    struct A {
        uint64_t u64;
        std::max_align_t _pad;
    };

    A *a = ar.alloc<A>();
    defer {
        ar.free_aligned(a);
    };

    a->u64 = 42;

    EXPECT_EQ((size_t)a % alignof(std::max_align_t), 0);
}
