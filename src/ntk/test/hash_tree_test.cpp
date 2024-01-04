#include "ntk/hash_tree.h"

#include <gtest/gtest.h>

#include "ntk/logger.h"
#include "ntk/string.h"
#include "ntk/utils.h"

class HashTree : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});
    }

    void TearDown() override {
    }
};

struct MyElem {
    int key;
    char const *value;
};

TEST_F(HashTree, val_key) {
    nkht_type(MyElem) ht{};
    defer {
        nkht_free(&ht);
    };

    MyElem elem{};
    MyElem *found;

    elem = {42, "forty two"};
    EXPECT_FALSE(nkht_find_val(&ht, 42));
    found = nkht_insert_val(&ht, elem);
    EXPECT_TRUE(nkht_find_val(&ht, 42));
    EXPECT_EQ(found->key, 42);
    EXPECT_STREQ(found->value, "forty two");

    elem = {1, "one"};
    EXPECT_FALSE(nkht_find_val(&ht, 1));
    found = nkht_insert_val(&ht, elem);
    EXPECT_TRUE(nkht_find_val(&ht, 1));
    EXPECT_EQ(found->key, 1);
    EXPECT_STREQ(found->value, "one");

    elem = {2, "two"};
    EXPECT_FALSE(nkht_find_val(&ht, 2));
    found = nkht_insert_val(&ht, elem);
    EXPECT_TRUE(nkht_find_val(&ht, 2));
    EXPECT_EQ(found->key, 2);
    EXPECT_STREQ(found->value, "two");

    elem = {2, "asdasdasd"};
    EXPECT_TRUE(nkht_find_val(&ht, 2));
    found = nkht_insert_val(&ht, elem);
    EXPECT_EQ(found->key, 2);
    EXPECT_STREQ(found->value, "two");

    elem = {41, "asdasdasd"};
    EXPECT_FALSE(nkht_find_val(&ht, 41));
    found = nkht_insert_val(&ht, elem);
    EXPECT_TRUE(nkht_find_val(&ht, 41));
    EXPECT_EQ(found->key, 41);
    EXPECT_STREQ(found->value, "asdasdasd");

    elem = {42, "asdasdasd"};
    EXPECT_TRUE(nkht_find_val(&ht, 42));
    found = nkht_insert_val(&ht, elem);
    EXPECT_EQ(found->key, 42);
    EXPECT_STREQ(found->value, "forty two");
}

struct MyElemWithStr {
    nks key;
    int value;
};

TEST_F(HashTree, str_key) {
    nkht_type(MyElemWithStr) ht{};
    defer {
        nkht_free(&ht);
    };

    MyElemWithStr elem{};
    MyElemWithStr *found;

    elem = {nk_cs2s("forty two"), 42};
    EXPECT_FALSE(nkht_find_str(&ht, nk_cs2s("forty two")));
    found = nkht_insert_str(&ht, elem);
    EXPECT_TRUE(nkht_find_str(&ht, nk_cs2s("forty two")));
    EXPECT_EQ(found->key, "forty two");
    EXPECT_EQ(found->value, 42);

    elem = {nk_cs2s("one"), 1};
    EXPECT_FALSE(nkht_find_str(&ht, nk_cs2s("one")));
    found = nkht_insert_str(&ht, elem);
    EXPECT_TRUE(nkht_find_str(&ht, nk_cs2s("one")));
    EXPECT_EQ(found->key, "one");
    EXPECT_EQ(found->value, 1);

    elem = {nk_cs2s("two"), 2};
    EXPECT_FALSE(nkht_find_str(&ht, nk_cs2s("two")));
    found = nkht_insert_str(&ht, elem);
    EXPECT_TRUE(nkht_find_str(&ht, nk_cs2s("two")));
    EXPECT_EQ(found->key, "two");
    EXPECT_EQ(found->value, 2);

    elem = {nk_cs2s("two"), 123};
    EXPECT_TRUE(nkht_find_str(&ht, nk_cs2s("two")));
    found = nkht_insert_str(&ht, elem);
    EXPECT_EQ(found->key, "two");
    EXPECT_EQ(found->value, 2);

    elem = {nk_cs2s("forty one"), 41};
    EXPECT_FALSE(nkht_find_str(&ht, nk_cs2s("forty one")));
    found = nkht_insert_str(&ht, elem);
    EXPECT_TRUE(nkht_find_str(&ht, nk_cs2s("forty one")));
    EXPECT_EQ(found->key, "forty one");
    EXPECT_EQ(found->value, 41);

    elem = {nk_cs2s("forty two"), 123};
    EXPECT_TRUE(nkht_find_str(&ht, nk_cs2s("forty two")));
    found = nkht_insert_str(&ht, elem);
    EXPECT_EQ(found->key, "forty two");
    EXPECT_EQ(found->value, 42);
}
