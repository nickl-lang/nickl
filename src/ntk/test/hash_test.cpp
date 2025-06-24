#include "ntk/hash.h"

#include <cstdio>

#include <gtest/gtest.h>

static void printHash128(NkHash128 hash) {
    for (u8 byte : hash.bytes) {
        printf("%02x", byte);
    }
}

#define EXPECT_HASH_EQ(lhs, rhs) EXPECT_TRUE(nk_hash128_equal((lhs), (rhs)))

TEST(hash, basic128) {
    char const *str = "hello world";

    auto const hash = nk_hash128((u8 const *)str, strlen(str));

    std::cout << "hash: ";
    printHash128(hash);
    std::cout << std::endl;

    EXPECT_HASH_EQ(
        hash,
        (NkHash128{0xd7, 0x49, 0x81, 0xef, 0xa7, 0x0a, 0x0c, 0x88, 0x0b, 0x8d, 0x8c, 0x19, 0x85, 0xd0, 0x75, 0xdb}));
}

TEST(hash, state128) {
    char const *str = "hello world";

    auto const hash_expected = nk_hash128((u8 const *)str, strlen(str));

    NkHashState state;
    nk_hash128_init(&state);
    nk_hash128_update(&state, (u8 const *)str, strlen(str));

    auto const hash_actual = nk_hash128_finalize(&state);

    EXPECT_HASH_EQ(hash_actual, hash_expected);
}

TEST(hash, basic64) {
    char const *str0 = "hello world";
    char const *str1 = "hello world";
    char const *str2 = "hello-world";

    auto const hash0 = nk_hash64((u8 const *)str0, strlen(str0));
    auto const hash1 = nk_hash64((u8 const *)str1, strlen(str1));
    auto const hash2 = nk_hash64((u8 const *)str2, strlen(str2));

    EXPECT_EQ(hash0, hash1);
    EXPECT_NE(hash0, hash2);
}

TEST(hash, value64) {
    double const val = 3.14;
    EXPECT_EQ(nk_hash64((u8 *)&val, sizeof(val)), nk_hash64_val(val));
}
