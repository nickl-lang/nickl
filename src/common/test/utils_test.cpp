#include "nk/common/utils.h"

#include <gtest/gtest.h>

TEST(utils, log2_u32) {
    EXPECT_EQ(log2u32(1), 0);
    for (std::size_t i = 1; i < 32; i++) {
        EXPECT_EQ(log2u32((1ul << i) - 1), i - 1);
        EXPECT_EQ(log2u32((1ul << i) + 0), i);
        EXPECT_EQ(log2u32((1ul << i) + 1), i);
    }
    EXPECT_EQ(log2u32(-1), 31);
}

TEST(utils, log2_u64) {
    EXPECT_EQ(log2u64(1), 0);
    for (std::size_t i = 1; i < 64; i++) {
        EXPECT_EQ(log2u64((1ul << i) - 1), i - 1);
        EXPECT_EQ(log2u64((1ul << i) + 0), i);
        EXPECT_EQ(log2u64((1ul << i) + 1), i);
    }
    EXPECT_EQ(log2u64(-1), 63);
}
