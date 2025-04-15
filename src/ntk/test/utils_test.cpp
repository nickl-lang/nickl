#include "ntk/utils.h"

#include <gtest/gtest.h>

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
