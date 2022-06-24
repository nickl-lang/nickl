#include "nk/common/array.hpp"

#include <cstring>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/utils.hpp"

class array : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(array, init) {
    Array<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    ar.reserve(1);

    EXPECT_EQ(ar.size, 0);
    EXPECT_EQ(ar.capacity, 1);
    EXPECT_NE(ar.data, nullptr);
}

TEST_F(array, basic) {
    using ValueType = uint16_t;

    Array<ValueType> ar{};
    defer {
        ar.deinit();
    };

    *ar.push() = 0;
    *ar.push() = 1;
    *ar.push() = 2;

    EXPECT_EQ(ar.size, 3);
    EXPECT_EQ(ar.capacity, 4);

    for (size_t i = 0; i < ar.size; i++) {
        EXPECT_EQ(ar[i], i);
    }

    ar.clear();

    EXPECT_EQ(ar.size, 0);
    EXPECT_EQ(ar.capacity, 4);
}

TEST_F(array, capacity) {
    Array<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    for (uint8_t i = 1; i < 60; i++) {
        *ar.push() = i;
        EXPECT_EQ(ar.capacity, ceilToPowerOf2(i));
    }
}

TEST_F(array, zero_capacity) {
    Array<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    ar.reserve(0);

    EXPECT_EQ(ar.capacity, 0);
    EXPECT_EQ(ar.size, 0);
    EXPECT_EQ(ar.data, nullptr);

    ar.push();

    EXPECT_EQ(ar.capacity, 1);
    EXPECT_EQ(ar.size, 1);
    EXPECT_NE(ar.data, nullptr);
}

TEST_F(array, zero_init) {
    Array<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    *ar.push() = 42;

    EXPECT_EQ(ar.capacity, 1);
    EXPECT_EQ(ar.size, 1);
    EXPECT_EQ(ar[0], 42);
}

TEST_F(array, append) {
    Array<char> ar{};
    defer {
        ar.deinit();
    };

    auto const c_test_str = cs2s("hello world");
    ar.append(c_test_str);

    EXPECT_EQ(ar.capacity, ceilToPowerOf2(c_test_str.size));
    EXPECT_EQ(ar.size, c_test_str.size);
    EXPECT_EQ(std_str(ar), std_str(c_test_str));
}

TEST_F(array, align) {
    size_t const c_align = 3;

    Array<double> ar{};
    defer {
        ar.deinit();
    };

    auto ptr = ar.push_aligned(c_align);
    EXPECT_EQ((size_t)ptr.data % (c_align * alignof(decltype(ar)::value_type)), 0);
}
