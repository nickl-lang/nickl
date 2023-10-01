#define NKAR_INIT_CAP 1

#include "ntk/array.h"

#include <cstring>

#include <gtest/gtest.h>

#include "ntk/logger.h"
#include "ntk/string.hpp"
#include "ntk/utils.hpp"

class array : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});
    }

    void TearDown() override {
    }
};

TEST_F(array, init) {
    nkar_type(uint8_t) ar{};
    defer {
        nkar_free(&ar);
    };

    nkar_reserve(&ar, 1);

    EXPECT_EQ(ar.size, 0);
    EXPECT_EQ(ar.capacity, 1);
    EXPECT_NE(ar.data, nullptr);
}

TEST_F(array, basic) {
    nkar_type(uint16_t) ar{};
    defer {
        nkar_free(&ar);
    };

    nkar_append(&ar, 0);
    nkar_append(&ar, 1);
    nkar_append(&ar, 2);

    EXPECT_EQ(ar.size, 3);
    EXPECT_EQ(ar.capacity, 4);

    for (size_t i = 0; i < ar.size; i++) {
        EXPECT_EQ(ar.data[i], i);
    }

    nkar_clear(&ar);

    EXPECT_EQ(ar.size, 0);
    EXPECT_EQ(ar.capacity, 4);
}

TEST_F(array, capacity) {
    nkar_type(uint8_t) ar{};
    defer {
        nkar_free(&ar);
    };

    for (uint8_t i = 1; i < 60; i++) {
        nkar_append(&ar, i);
        EXPECT_EQ(ar.capacity, ceilToPowerOf2(i));
    }
}

TEST_F(array, zero_capacity) {
    nkar_type(uint8_t) ar{};
    defer {
        nkar_free(&ar);
    };

    nkar_reserve(&ar, 0);

    EXPECT_EQ(ar.capacity, 1);
    EXPECT_EQ(ar.size, 0);
    EXPECT_NE(ar.data, nullptr);

    nkar_append(&ar, 0);

    EXPECT_EQ(ar.capacity, 1);
    EXPECT_EQ(ar.size, 1);
    EXPECT_NE(ar.data, nullptr);
}

TEST_F(array, zero_init) {
    nkar_type(uint8_t) ar{};
    defer {
        nkar_free(&ar);
    };

    nkar_append(&ar, 42);

    EXPECT_EQ(ar.capacity, 1);
    EXPECT_EQ(ar.size, 1);
    EXPECT_EQ(*ar.data, 42);
}

TEST_F(array, append) {
    nkar_type(char) ar{};
    defer {
        nkar_free(&ar);
    };

    auto const c_test_str = nk_cs2s("hello world");
    nkar_append_many(&ar, c_test_str.data, c_test_str.size);

    EXPECT_EQ(ar.capacity, ceilToPowerOf2(c_test_str.size));
    EXPECT_EQ(ar.size, c_test_str.size);
    EXPECT_EQ(std_str({nkav_init(ar)}), std_str(c_test_str));
}

TEST_F(array, multiple_reserves) {
    nkar_type(int) ar{};
    defer {
        nkar_free(&ar);
    };

    size_t sz = 1;

    sz *= 10;
    nkar_reserve(&ar, sz);
    sz *= 10;
    nkar_reserve(&ar, sz);
    sz *= 10;
    nkar_reserve(&ar, sz);

    ar.size += sz;
    memset(ar.data, 0, sz * sizeof(ar.data[0]));

    EXPECT_EQ(ar.size, sz);
    EXPECT_EQ(ar.capacity, ceilToPowerOf2(sz));
}
