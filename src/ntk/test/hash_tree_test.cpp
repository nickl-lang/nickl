#include "ntk/hash_tree.h"

#include <gtest/gtest.h>

#include "ntk/arena.h"
#include "ntk/log.h"
#include "ntk/string.h"
#include "ntk/utils.h"

class HashTree : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});
    }

    void TearDown() override {
    }
};

struct int2cstr_kv {
    int key;
    char const *val;
};

int const *int2cstr_kv_GetKey(int2cstr_kv const *elem) {
    return &elem->key;
}

u64 int_hash(int const key) {
    return nk_hashArray((u8 const *)&key, (u8 const *)&key + sizeof(key));
}

bool int_equal(int const lhs, int const rhs) {
    return lhs == rhs;
}

NK_HASH_TREE_DEFINE(int2cstr_ht, int2cstr_kv, int, int2cstr_kv_GetKey, int_hash, int_equal);

TEST_F(HashTree, val_key) {
    int2cstr_ht ht{};
    defer {
        int2cstr_ht_free(&ht);
    };

    int2cstr_kv elem{};
    int2cstr_kv *found;

    elem = {42, "forty two"};
    EXPECT_FALSE(int2cstr_ht_find(&ht, 42));
    found = int2cstr_ht_insert(&ht, elem);
    EXPECT_TRUE(int2cstr_ht_find(&ht, 42));
    EXPECT_EQ(found->key, 42);
    EXPECT_STREQ(found->val, "forty two");

    elem = {1, "one"};
    EXPECT_FALSE(int2cstr_ht_find(&ht, 1));
    found = int2cstr_ht_insert(&ht, elem);
    EXPECT_TRUE(int2cstr_ht_find(&ht, 1));
    EXPECT_EQ(found->key, 1);
    EXPECT_STREQ(found->val, "one");

    elem = {2, "two"};
    EXPECT_FALSE(int2cstr_ht_find(&ht, 2));
    found = int2cstr_ht_insert(&ht, elem);
    EXPECT_TRUE(int2cstr_ht_find(&ht, 2));
    EXPECT_EQ(found->key, 2);
    EXPECT_STREQ(found->val, "two");

    elem = {2, "asdasdasd"};
    EXPECT_TRUE(int2cstr_ht_find(&ht, 2));
    found = int2cstr_ht_insert(&ht, elem);
    EXPECT_EQ(found->key, 2);
    EXPECT_STREQ(found->val, "two");

    elem = {41, "asdasdasd"};
    EXPECT_FALSE(int2cstr_ht_find(&ht, 41));
    found = int2cstr_ht_insert(&ht, elem);
    EXPECT_TRUE(int2cstr_ht_find(&ht, 41));
    EXPECT_EQ(found->key, 41);
    EXPECT_STREQ(found->val, "asdasdasd");

    elem = {42, "asdasdasd"};
    EXPECT_TRUE(int2cstr_ht_find(&ht, 42));
    found = int2cstr_ht_insert(&ht, elem);
    EXPECT_EQ(found->key, 42);
    EXPECT_STREQ(found->val, "forty two");
}

struct str2int_kv {
    NkString key;
    int val;
};

NkString const *str2int_kv_GetKey(str2int_kv const *elem) {
    return &elem->key;
}

NK_HASH_TREE_DEFINE(str2int, str2int_kv, NkString, str2int_kv_GetKey, nks_hash, nks_equal);

TEST_F(HashTree, str_key) {
    str2int ht{};
    defer {
        str2int_free(&ht);
    };

    str2int_kv elem{};
    str2int_kv *found;

    elem = {nk_cs2s("forty two"), 42};
    EXPECT_FALSE(str2int_find(&ht, nk_cs2s("forty two")));
    found = str2int_insert(&ht, elem);
    EXPECT_TRUE(str2int_find(&ht, nk_cs2s("forty two")));
    EXPECT_EQ(found->key, "forty two");
    EXPECT_EQ(found->val, 42);

    elem = {nk_cs2s("one"), 1};
    EXPECT_FALSE(str2int_find(&ht, nk_cs2s("one")));
    found = str2int_insert(&ht, elem);
    EXPECT_TRUE(str2int_find(&ht, nk_cs2s("one")));
    EXPECT_EQ(found->key, "one");
    EXPECT_EQ(found->val, 1);

    elem = {nk_cs2s("two"), 2};
    EXPECT_FALSE(str2int_find(&ht, nk_cs2s("two")));
    found = str2int_insert(&ht, elem);
    EXPECT_TRUE(str2int_find(&ht, nk_cs2s("two")));
    EXPECT_EQ(found->key, "two");
    EXPECT_EQ(found->val, 2);

    elem = {nk_cs2s("two"), 123};
    EXPECT_TRUE(str2int_find(&ht, nk_cs2s("two")));
    found = str2int_insert(&ht, elem);
    EXPECT_EQ(found->key, "two");
    EXPECT_EQ(found->val, 2);

    elem = {nk_cs2s("forty one"), 41};
    EXPECT_FALSE(str2int_find(&ht, nk_cs2s("forty one")));
    found = str2int_insert(&ht, elem);
    EXPECT_TRUE(str2int_find(&ht, nk_cs2s("forty one")));
    EXPECT_EQ(found->key, "forty one");
    EXPECT_EQ(found->val, 41);

    elem = {nk_cs2s("forty two"), 123};
    EXPECT_TRUE(str2int_find(&ht, nk_cs2s("forty two")));
    found = str2int_insert(&ht, elem);
    EXPECT_EQ(found->key, "forty two");
    EXPECT_EQ(found->val, 42);
}

TEST_F(HashTree, alignment) {
    NkArena arena{};
    auto alloc = nk_arena_getAllocator(&arena);
    defer {
        nk_arena_free(&arena);
    };

    str2int ht{};
    ht.alloc = alloc;

    nk_arena_alloc(&arena, 1);
    str2int_insert(&ht, str2int_kv{nk_cs2s("one"), 42});
    str2int_insert(&ht, str2int_kv{nk_cs2s("two"), 42});
}
