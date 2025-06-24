#include "ntk/hash_tree.h"

#include <gtest/gtest.h>

#include "ntk/arena.h"
#include "ntk/hash.h"
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

NkHash64 int_hash(int const key) {
    return nk_hash64_val(key);
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
    EXPECT_FALSE(int2cstr_ht_findItem(&ht, 42));
    found = int2cstr_ht_insertItem(&ht, elem);
    EXPECT_TRUE(int2cstr_ht_findItem(&ht, 42));
    EXPECT_EQ(found->key, 42);
    EXPECT_STREQ(found->val, "forty two");

    elem = {1, "one"};
    EXPECT_FALSE(int2cstr_ht_findItem(&ht, 1));
    found = int2cstr_ht_insertItem(&ht, elem);
    EXPECT_TRUE(int2cstr_ht_findItem(&ht, 1));
    EXPECT_EQ(found->key, 1);
    EXPECT_STREQ(found->val, "one");

    elem = {2, "two"};
    EXPECT_FALSE(int2cstr_ht_findItem(&ht, 2));
    found = int2cstr_ht_insertItem(&ht, elem);
    EXPECT_TRUE(int2cstr_ht_findItem(&ht, 2));
    EXPECT_EQ(found->key, 2);
    EXPECT_STREQ(found->val, "two");

    elem = {2, "asdasdasd"};
    EXPECT_TRUE(int2cstr_ht_findItem(&ht, 2));
    found = int2cstr_ht_insertItem(&ht, elem);
    EXPECT_EQ(found->key, 2);
    EXPECT_STREQ(found->val, "two");

    elem = {41, "asdasdasd"};
    EXPECT_FALSE(int2cstr_ht_findItem(&ht, 41));
    found = int2cstr_ht_insertItem(&ht, elem);
    EXPECT_TRUE(int2cstr_ht_findItem(&ht, 41));
    EXPECT_EQ(found->key, 41);
    EXPECT_STREQ(found->val, "asdasdasd");

    elem = {42, "asdasdasd"};
    EXPECT_TRUE(int2cstr_ht_findItem(&ht, 42));
    found = int2cstr_ht_insertItem(&ht, elem);
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
    EXPECT_FALSE(str2int_findItem(&ht, nk_cs2s("forty two")));
    found = str2int_insertItem(&ht, elem);
    EXPECT_TRUE(str2int_findItem(&ht, nk_cs2s("forty two")));
    EXPECT_EQ(found->key, "forty two");
    EXPECT_EQ(found->val, 42);

    elem = {nk_cs2s("one"), 1};
    EXPECT_FALSE(str2int_findItem(&ht, nk_cs2s("one")));
    found = str2int_insertItem(&ht, elem);
    EXPECT_TRUE(str2int_findItem(&ht, nk_cs2s("one")));
    EXPECT_EQ(found->key, "one");
    EXPECT_EQ(found->val, 1);

    elem = {nk_cs2s("two"), 2};
    EXPECT_FALSE(str2int_findItem(&ht, nk_cs2s("two")));
    found = str2int_insertItem(&ht, elem);
    EXPECT_TRUE(str2int_findItem(&ht, nk_cs2s("two")));
    EXPECT_EQ(found->key, "two");
    EXPECT_EQ(found->val, 2);

    elem = {nk_cs2s("two"), 123};
    EXPECT_TRUE(str2int_findItem(&ht, nk_cs2s("two")));
    found = str2int_insertItem(&ht, elem);
    EXPECT_EQ(found->key, "two");
    EXPECT_EQ(found->val, 2);

    elem = {nk_cs2s("forty one"), 41};
    EXPECT_FALSE(str2int_findItem(&ht, nk_cs2s("forty one")));
    found = str2int_insertItem(&ht, elem);
    EXPECT_TRUE(str2int_findItem(&ht, nk_cs2s("forty one")));
    EXPECT_EQ(found->key, "forty one");
    EXPECT_EQ(found->val, 41);

    elem = {nk_cs2s("forty two"), 123};
    EXPECT_TRUE(str2int_findItem(&ht, nk_cs2s("forty two")));
    found = str2int_insertItem(&ht, elem);
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
    str2int_insertItem(&ht, str2int_kv{nk_cs2s("one"), 42});
    str2int_insertItem(&ht, str2int_kv{nk_cs2s("two"), 42});
}

TEST_F(HashTree, not_found) {
    str2int ht{};
    defer {
        str2int_free(&ht);
    };

    EXPECT_FALSE(str2int_findItem(&ht, nk_cs2s("one")));
}

NK_HASH_TREE_DEFINE_K(IntSet, int, int_hash, int_equal);

TEST_F(HashTree, hash_set) {
    IntSet set{};
    defer {
        IntSet_free(&set);
    };

    IntSet_insert(&set, 3);
    IntSet_insert(&set, 2);
    IntSet_insert(&set, 2);
    IntSet_insert(&set, 1);
    IntSet_insert(&set, 1);
    IntSet_insert(&set, 1);
    IntSet_insert(&set, 3);
    IntSet_insert(&set, 3);
    IntSet_insert(&set, 2);

    EXPECT_TRUE(IntSet_find(&set, 1));
    EXPECT_TRUE(IntSet_find(&set, 2));
    EXPECT_TRUE(IntSet_find(&set, 3));
    EXPECT_FALSE(IntSet_find(&set, 4));
    EXPECT_FALSE(IntSet_find(&set, 5));
}

NK_HASH_TREE_DEFINE_KV(IntStringMap, int, NkString, int_hash, int_equal);

TEST_F(HashTree, hash_map) {
    IntStringMap map{};
    defer {
        IntStringMap_free(&map);
    };

    IntStringMap_insert(&map, 2, nk_cs2s("two"));
    IntStringMap_insert(&map, 1, nk_cs2s("one"));
    IntStringMap_insert(&map, 2, nk_cs2s("___"));
    IntStringMap_insert(&map, 3, nk_cs2s("three"));

    {
        auto found = IntStringMap_find(&map, 1);
        ASSERT_TRUE(found);
        EXPECT_EQ(*found, "one");
    }
    {
        auto found = IntStringMap_find(&map, 2);
        ASSERT_TRUE(found);
        EXPECT_EQ(*found, "two");
    }
    {
        auto found = IntStringMap_find(&map, 3);
        ASSERT_TRUE(found);
        EXPECT_EQ(*found, "three");
    }
    EXPECT_FALSE(IntStringMap_find(&map, 4));
    EXPECT_FALSE(IntStringMap_find(&map, 5));
}
