#include "ntk/hash.h"

#include <cstdio>

#include <gtest/gtest.h>

#define EXPECT_HASH_EQ(lhs, rhs)               \
    do {                                       \
        EXPECT_EQ(lhs.parts[0], rhs.parts[0]); \
        EXPECT_EQ(lhs.parts[1], rhs.parts[1]); \
        EXPECT_EQ(lhs.parts[2], rhs.parts[2]); \
        EXPECT_EQ(lhs.parts[3], rhs.parts[3]); \
    } while (0)

void printHash4(NkHash hash) {
    printf("%08x%08x%08x%08x", hash.parts[0], hash.parts[1], hash.parts[2], hash.parts[3]);
}

TEST(hash, basic) {
    char const *str = "hello world";

    auto hash = nk_hash((u8 const *)str, strlen(str));

    std::cout << "hash: ";
    printHash4(hash);
    std::cout << std::endl;

    EXPECT_HASH_EQ(hash, (NkHash{{0xd74981ef, 0xa70a0c88, 0x0b8d8c19, 0x85d075db}}));
}

TEST(hash, state) {
    char const *str = "hello world";

    auto const hash_expected = nk_hash((u8 const *)str, strlen(str));

    NkHashState state;
    nk_hash_init(&state);
    nk_hash_update(&state, (u8 const *)str, strlen(str));

    auto const hash_actual = nk_hash_finalize(&state);

    EXPECT_HASH_EQ(hash_actual, hash_expected);
}

TEST(hash, value) {
    double const val = 3.14;

    EXPECT_HASH_EQ(nk_hash((u8 *)&val, sizeof(val)), nk_hash_val(val));
}
