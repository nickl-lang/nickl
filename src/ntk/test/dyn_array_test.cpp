#define NKDA_INITIAL_CAPACITY 1

#include "ntk/dyn_array.h"

#include <cstring>

#include <gtest/gtest.h>

#include "ntk/log.h"
#include "ntk/string.h"
#include "ntk/utils.h"

class dyn_array : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});
    }

    void TearDown() override {
    }
};

TEST_F(dyn_array, init) {
    NkDynArray(u8) ar{};
    defer {
        nkda_free(&ar);
    };

    nkda_reserve(&ar, 1);

    EXPECT_EQ(ar.size, 0);
    EXPECT_EQ(ar.capacity, 1);
    EXPECT_NE(ar.data, nullptr);
}

TEST_F(dyn_array, basic) {
    NkDynArray(u16) ar{};
    defer {
        nkda_free(&ar);
    };

    nkda_append(&ar, 0);
    nkda_append(&ar, 1);
    nkda_append(&ar, 2);

    EXPECT_EQ(ar.size, 3);
    EXPECT_EQ(ar.capacity, 4);

    for (usize i = 0; i < ar.size; i++) {
        EXPECT_EQ(ar.data[i], i);
    }

    nkda_clear(&ar);

    EXPECT_EQ(ar.size, 0);
    EXPECT_EQ(ar.capacity, 4);
}

TEST_F(dyn_array, capacity) {
    NkDynArray(u8) ar{};
    defer {
        nkda_free(&ar);
    };

    for (u8 i = 1; i < 60; i++) {
        nkda_append(&ar, i);
        EXPECT_EQ(ar.capacity, nk_ceilToPowerOf2(i));
    }
}

TEST_F(dyn_array, zero_capacity) {
    NkDynArray(u8) ar{};
    defer {
        nkda_free(&ar);
    };

    nkda_reserve(&ar, 0);

    EXPECT_EQ(ar.capacity, 1);
    EXPECT_EQ(ar.size, 0);
    EXPECT_NE(ar.data, nullptr);

    nkda_append(&ar, 0);

    EXPECT_EQ(ar.capacity, 1);
    EXPECT_EQ(ar.size, 1);
    EXPECT_NE(ar.data, nullptr);
}

TEST_F(dyn_array, zero_init) {
    NkDynArray(u8) ar{};
    defer {
        nkda_free(&ar);
    };

    nkda_append(&ar, 42);

    EXPECT_EQ(ar.capacity, 1);
    EXPECT_EQ(ar.size, 1);
    EXPECT_EQ(*ar.data, 42);
}

TEST_F(dyn_array, append) {
    NkDynArray(char) ar{};
    defer {
        nkda_free(&ar);
    };

    auto const c_test_str = nk_cs2s("hello world");
    nkda_appendMany(&ar, c_test_str.data, c_test_str.size);

    EXPECT_EQ(ar.capacity, nk_ceilToPowerOf2(c_test_str.size));
    EXPECT_EQ(ar.size, c_test_str.size);
    EXPECT_EQ(nk_s2stdStr({NKS_INIT(ar)}), nk_s2stdStr(c_test_str));
}

TEST_F(dyn_array, multiple_reserves) {
    NkDynArray(int) ar{};
    defer {
        nkda_free(&ar);
    };

    usize sz = 1;

    sz *= 10;
    nkda_reserve(&ar, sz);
    sz *= 10;
    nkda_reserve(&ar, sz);
    sz *= 10;
    nkda_reserve(&ar, sz);

    ar.size += sz;
    memset(ar.data, 0, sz * sizeof(ar.data[0]));

    EXPECT_EQ(ar.size, sz);
    EXPECT_EQ(ar.capacity, nk_ceilToPowerOf2(sz));
}
