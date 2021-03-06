#include "nk/common/array.hpp"

#include <cstring>

#include <gtest/gtest.h>

#include "nk/common/utils.hpp"

class array : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(array, init) {
    auto ar = Array<uint8_t>::create(1);
    defer {
        ar.deinit();
    };

    EXPECT_EQ(ar.size, 0);
    EXPECT_EQ(ar.capacity, 1);
    EXPECT_NE(ar.data, nullptr);
}

TEST_F(array, basic) {
    using ValueType = uint16_t;

    auto ar = Array<ValueType>::create();
    defer {
        ar.deinit();
    };

    ar.push() = 0;
    ar.push() = 1;
    ar.push() = 2;

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
    auto ar = Array<uint8_t>::create();
    defer {
        ar.deinit();
    };

    for (uint8_t i = 1; i < 60; i++) {
        ar.push() = i;
        EXPECT_EQ(ar.capacity, ceilToPowerOf2(i));
    }
}

TEST_F(array, zero_capacity) {
    auto ar = Array<uint8_t>::create(0);
    defer {
        ar.deinit();
    };

    EXPECT_EQ(ar.capacity, 0);
    EXPECT_EQ(ar.size, 0);
    EXPECT_EQ(ar.data, nullptr);

    ar.push();

    EXPECT_EQ(ar.capacity, 1);
    EXPECT_EQ(ar.size, 1);
    EXPECT_NE(ar.data, nullptr);
}

TEST_F(array, zero_init) {
    Array<uint8_t> ar;
    defer {
        ar.deinit();
    };

    std::memset(&ar, 0, sizeof(ar));

    ar.push() = 42;

    EXPECT_EQ(ar.capacity, 1);
    EXPECT_EQ(ar.size, 1);
    EXPECT_EQ(ar[0], 42);
}
