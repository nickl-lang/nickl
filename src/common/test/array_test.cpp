#include "nk/common/array.hpp"

#include <cstring>

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/string.hpp"
#include "nk/common/utils.hpp"

class array : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({NkLog_Trace, NkLog_Color_Auto});
    }

    void TearDown() override {
    }
};

TEST_F(array, init) {
    NkArray<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    EXPECT_TRUE(ar.reserve(1));

    EXPECT_EQ(ar.size(), 0);
    EXPECT_EQ(ar.capacity(), 1);
    EXPECT_NE(ar.data(), nullptr);
}

TEST_F(array, basic) {
    using ValueType = uint16_t;

    NkArray<ValueType> ar{};
    defer {
        ar.deinit();
    };

    *ar.push() = 0;
    *ar.push() = 1;
    *ar.push() = 2;

    EXPECT_EQ(ar.size(), 3);
    EXPECT_EQ(ar.capacity(), 4);

    for (size_t i = 0; i < ar.size(); i++) {
        EXPECT_EQ(ar[i], i);
    }

    ar.clear();

    EXPECT_EQ(ar.size(), 0);
    EXPECT_EQ(ar.capacity(), 4);
}

TEST_F(array, capacity) {
    NkArray<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    for (uint8_t i = 1; i < 60; i++) {
        *ar.push() = i;
        EXPECT_EQ(ar.capacity(), ceilToPowerOf2(i));
    }
}

TEST_F(array, zero_capacity) {
    NkArray<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    EXPECT_TRUE(ar.reserve(0));

    EXPECT_EQ(ar.capacity(), 0);
    EXPECT_EQ(ar.size(), 0);
    EXPECT_EQ(ar.data(), nullptr);

    ar.push();

    EXPECT_EQ(ar.capacity(), 1);
    EXPECT_EQ(ar.size(), 1);
    EXPECT_NE(ar.data(), nullptr);
}

TEST_F(array, zero_init) {
    NkArray<uint8_t> ar{};
    defer {
        ar.deinit();
    };

    *ar.push() = 42;

    EXPECT_EQ(ar.capacity(), 1);
    EXPECT_EQ(ar.size(), 1);
    EXPECT_EQ(*ar, 42);
}

TEST_F(array, append) {
    NkArray<char> ar{};
    defer {
        ar.deinit();
    };

    auto const c_test_str = cs2s("hello world");
    ar.append({c_test_str.data, c_test_str.size});

    EXPECT_EQ(ar.capacity(), ceilToPowerOf2(c_test_str.size));
    EXPECT_EQ(ar.size(), c_test_str.size);
    EXPECT_EQ(std_str({ar.data(), ar.size()}), std_str(c_test_str));
}

TEST_F(array, multiple_reserves) {
    NkArray<int> ar{};
    defer {
        ar.deinit();
    };

    size_t sz = 1;

    EXPECT_TRUE(ar.reserve(sz *= 10));
    EXPECT_TRUE(ar.reserve(sz *= 10));
    EXPECT_TRUE(ar.reserve(sz *= 10));

    auto data = ar.push(sz);
    std::memset(data.data(), 0, sz * sizeof(decltype(ar)::value_type));

    EXPECT_EQ(ar.size(), sz);
}
