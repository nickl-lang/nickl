#include "nk/common/log2arena.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/utils.hpp"

class log2arena : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(log2arena, basic) {
    using ValueType = uint64_t;

    Log2Arena<ValueType> ar{};
    defer {
        ar.deinit();
    };

    ar.reserve(1);

    EXPECT_EQ(ar.size, 0);

    *ar.push() = 4;
    *ar.push() = 5;
    *ar.push() = 42;

    EXPECT_EQ(ar.size, 3);

    EXPECT_EQ(ar.at(0), 4);
    EXPECT_EQ(ar.at(1), 5);
    EXPECT_EQ(ar.at(2), 42);
}

TEST_F(log2arena, many_small_pushes) {
    static constexpr size_t c_ic = 64;
    static constexpr size_t c_bytes_to_push = 1024;

    Log2Arena<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    ar.reserve(c_ic);

    for (size_t i = 0; i < c_bytes_to_push; i++) {
        ar.push();
    }
    EXPECT_EQ(ar.size, c_bytes_to_push);
}

TEST_F(log2arena, one_big_push) {
    static constexpr size_t c_ic = 64;
    static constexpr size_t c_bytes_to_push = 1024;

    Log2Arena<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    ar.reserve(c_ic);

    ar.push(c_bytes_to_push);
    EXPECT_EQ(ar.size, 2 * c_bytes_to_push - ceilToPowerOf2(c_ic));
}

TEST_F(log2arena, pop) {
    static constexpr size_t c_bytes_to_push = 64;

    {
        Log2Arena<uint8_t> ar{};
        defer {
            ar.deinit();
        };

        ar.reserve(1);

        ar.push(c_bytes_to_push);
        size_t size1 = ar.size;
        EXPECT_EQ(size1, c_bytes_to_push * 2 - 1);

        ar.pop(ar.size);

        ar.push(c_bytes_to_push);
        size_t size2 = ar.size;
        EXPECT_EQ(size2, c_bytes_to_push * 2 - 1);
    }

    {
        Log2Arena<uint8_t> ar{};
        defer {
            ar.deinit();
        };

        ar.reserve(1);

        for (size_t i = 0; i < c_bytes_to_push; i++) {
            ar.push();
        }
        size_t size1 = ar.size;
        EXPECT_EQ(size1, c_bytes_to_push);

        ar.pop(ar.size);

        for (size_t i = 0; i < c_bytes_to_push; i++) {
            ar.push();
        }
        size_t size2 = ar.size;
        EXPECT_EQ(size2, c_bytes_to_push);
    }

    {
        Log2Arena<uint8_t> ar{};
        defer {
            ar.deinit();
        };

        ar.reserve(32);

        ar.push(32);
        EXPECT_EQ(ar.size, 32);
        ar.push(64);
        EXPECT_EQ(ar.size, 96);
        ar.pop(64);
        EXPECT_EQ(ar.size, 32);
        ar.push(64);
        EXPECT_EQ(ar.size, 96);
    }

    {
        Log2Arena<uint8_t> ar{};
        defer {
            ar.deinit();
        };

        ar.reserve(32);

        ar.push(32);
        EXPECT_EQ(ar.size, 32);
        ar.pop(32);
        EXPECT_EQ(ar.size, 0);
        ar.push(32);
        EXPECT_EQ(ar.size, 32);
        ar.pop(32);
        EXPECT_EQ(ar.size, 0);
    }
}

TEST_F(log2arena, zero_init) {
    Log2Arena<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    *ar.push() = 42;
    EXPECT_EQ(ar.size, 1);
    EXPECT_EQ(ar[0], 42);
}

TEST_F(log2arena, align) {
    size_t const c_align = 3;

    Log2Arena<double> ar{};
    defer {
        ar.deinit();
    };

    auto ptr = ar.push_aligned(c_align);
    EXPECT_EQ((size_t)ptr.data % (c_align * alignof(decltype(ar)::value_type)), 0);
}
