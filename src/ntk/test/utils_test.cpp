#include "ntk/utils.h"

#include <gtest/gtest.h>

TEST(utils, roundUp) {
    EXPECT_EQ(nk_roundUp(1, 1), 1);
    EXPECT_EQ(nk_roundUp(1, 2), 2);
    EXPECT_EQ(nk_roundUp(2, 2), 2);
    EXPECT_EQ(nk_roundUp(3, 2), 4);
    EXPECT_EQ(nk_roundUp(4, 2), 4);
    EXPECT_EQ(nk_roundUp(4, 10), 10);
    EXPECT_EQ(nk_roundUp(9, 10), 10);
    EXPECT_EQ(nk_roundUp(10, 10), 10);
    EXPECT_EQ(nk_roundUp(11, 10), 20);
}

TEST(utils, alignToPowerOf2) {
    EXPECT_EQ(nk_alignToPowerOf2(1, 0), 0);
    EXPECT_EQ(nk_alignToPowerOf2(1, 1), 1);
    EXPECT_EQ(nk_alignToPowerOf2(1, 2), 2);
    EXPECT_EQ(nk_alignToPowerOf2(2, 2), 2);
    EXPECT_EQ(nk_alignToPowerOf2(3, 2), 4);
    EXPECT_EQ(nk_alignToPowerOf2(4, 2), 4);
    EXPECT_EQ(nk_alignToPowerOf2(4, 16), 16);
    EXPECT_EQ(nk_alignToPowerOf2(15, 16), 16);
    EXPECT_EQ(nk_alignToPowerOf2(16, 16), 16);
    EXPECT_EQ(nk_alignToPowerOf2(17, 16), 32);
    EXPECT_EQ(nk_alignToPowerOf2(0, 1024), 0);
    EXPECT_EQ(nk_alignToPowerOf2(1, 1024), 1024);
    EXPECT_EQ(nk_alignToPowerOf2(4095, 1024), 4096);
}

TEST(utils, log2_u32) {
    EXPECT_EQ(nk_log2u32(1), 0u);
    for (usize i = 1; i < 32; i++) {
        EXPECT_EQ(nk_log2u32((1ull << i) - 1), i - 1);
        EXPECT_EQ(nk_log2u32((1ull << i) + 0), i);
        EXPECT_EQ(nk_log2u32((1ull << i) + 1), i);
    }
    EXPECT_EQ(nk_log2u32(-1), 31u);
}

TEST(utils, log2_u64) {
    EXPECT_EQ(nk_log2u64(1), 0u);
    for (usize i = 1; i < 64; i++) {
        EXPECT_EQ(nk_log2u64((1ull << i) - 1), i - 1);
        EXPECT_EQ(nk_log2u64((1ull << i) + 0), i);
        EXPECT_EQ(nk_log2u64((1ull << i) + 1), i);
    }
    EXPECT_EQ(nk_log2u64(-1), 63u);
}
