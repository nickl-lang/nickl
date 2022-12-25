#include "nk/utils/utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <gtest/gtest.h>

TEST(utils, roundUp) {
    EXPECT_EQ(roundUp(0, 1), 0);
    EXPECT_EQ(roundUp(1, 1), 1);
    EXPECT_EQ(roundUp(2, 1), 2);
    EXPECT_EQ(roundUp(0, 2), 0);
    EXPECT_EQ(roundUp(1, 2), 2);
    EXPECT_EQ(roundUp(2, 2), 2);
    EXPECT_EQ(roundUp(3, 2), 4);
    EXPECT_EQ(roundUp(4, 2), 4);
    EXPECT_EQ(roundUp(5, 2), 6);
    EXPECT_EQ(roundUp(42, 100), 100);
    EXPECT_EQ(roundUp(142, 100), 200);
}

TEST(utils, roundUpSafe) {
    EXPECT_EQ(roundUpSafe(0, 1), 0);
    EXPECT_EQ(roundUpSafe(1, 1), 1);
    EXPECT_EQ(roundUpSafe(2, 1), 2);
    EXPECT_EQ(roundUpSafe(0, 2), 0);
    EXPECT_EQ(roundUpSafe(1, 2), 2);
    EXPECT_EQ(roundUpSafe(2, 2), 2);
    EXPECT_EQ(roundUpSafe(3, 2), 4);
    EXPECT_EQ(roundUpSafe(4, 2), 4);
    EXPECT_EQ(roundUpSafe(5, 2), 6);
    EXPECT_EQ(roundUpSafe(42, 100), 100);
    EXPECT_EQ(roundUpSafe(142, 100), 200);

    EXPECT_EQ(roundUpSafe(0, 0), 0);
    EXPECT_EQ(roundUpSafe(1, 0), 1);
    EXPECT_EQ(roundUpSafe(2, 0), 2);
    EXPECT_EQ(roundUpSafe(42, 0), 42);
}

TEST(utils, ceilToPowerOf2) {
    EXPECT_EQ(ceilToPowerOf2(0), 0);
    EXPECT_EQ(ceilToPowerOf2(1), 1);
    EXPECT_EQ(ceilToPowerOf2(2), 2);
    EXPECT_EQ(ceilToPowerOf2(3), 4);
    EXPECT_EQ(ceilToPowerOf2(4), 4);
    EXPECT_EQ(ceilToPowerOf2(5), 8);
    EXPECT_EQ(ceilToPowerOf2(32), 32);
    EXPECT_EQ(ceilToPowerOf2(42), 64);
    EXPECT_EQ(ceilToPowerOf2(142), 256);
    EXPECT_EQ(ceilToPowerOf2(1000), 1024);
}

TEST(utils, floorToPowerOf2) {
    EXPECT_EQ(floorToPowerOf2(0), 0);
    EXPECT_EQ(floorToPowerOf2(1), 1);
    EXPECT_EQ(floorToPowerOf2(2), 2);
    EXPECT_EQ(floorToPowerOf2(3), 2);
    EXPECT_EQ(floorToPowerOf2(4), 4);
    EXPECT_EQ(floorToPowerOf2(5), 4);
    EXPECT_EQ(floorToPowerOf2(32), 32);
    EXPECT_EQ(floorToPowerOf2(42), 32);
    EXPECT_EQ(floorToPowerOf2(142), 128);
    EXPECT_EQ(floorToPowerOf2(1000), 512);
}

TEST(utils, isZeroOrPowerOf2) {
    EXPECT_TRUE(isZeroOrPowerOf2(0));
    EXPECT_TRUE(isZeroOrPowerOf2(1));
    EXPECT_TRUE(isZeroOrPowerOf2(2));
    EXPECT_FALSE(isZeroOrPowerOf2(3));
    EXPECT_TRUE(isZeroOrPowerOf2(4));
    EXPECT_FALSE(isZeroOrPowerOf2(5));
    EXPECT_TRUE(isZeroOrPowerOf2(32));
    EXPECT_FALSE(isZeroOrPowerOf2(42));
    EXPECT_FALSE(isZeroOrPowerOf2(142));
    EXPECT_FALSE(isZeroOrPowerOf2(1000));
    EXPECT_TRUE(isZeroOrPowerOf2(1024));
}

TEST(utils, log2u) {
    for (uint32_t i = 0; i <= 2 << 13; i++) {
        EXPECT_EQ(log2u(i), static_cast<uint32_t>(std::log2(i)));
    }
}

TEST(utils, min_max) {
    for (int64_t x = -5; x <= 5; x++) {
        for (int64_t y = -5; y <= 5; y++) {
            EXPECT_EQ(mini(x, y), std::min(x, y));
            EXPECT_EQ(maxi(x, y), std::max(x, y));
        }
    }

    for (uint64_t x = 0; x <= 5; x++) {
        for (uint64_t y = 0; y <= 5; y++) {
            EXPECT_EQ(minu(x, y), std::min(x, y));
            EXPECT_EQ(maxu(x, y), std::max(x, y));
        }
    }
}

TEST(utils, hash) {
    std::string str1{"Hello, World!"};
    std::string str2{"Hello, World!"};

    auto const _hash_string = [](std::string const &str) {
        return hash_cstrn(str.c_str(), str.size());
    };

    EXPECT_EQ(str1, str2);
    EXPECT_EQ(_hash_string(str1), _hash_string(str2));

    str2[0] = 'A';

    EXPECT_NE(_hash_string(str1), _hash_string(str2));
}

TEST(utils, defer_test) {
    std::string msg;

    {
        defer {
            msg += "!";
        };
        defer {
            msg += "World";
        };
        defer {
            msg += ", ";
        };
        defer {
            msg += "Hello";
        };
    }

    ASSERT_EQ(msg, "Hello, World!");
}
