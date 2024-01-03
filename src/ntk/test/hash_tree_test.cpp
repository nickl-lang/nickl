#include "ntk/hash_tree.h"

#include <gtest/gtest.h>

#include "ntk/logger.h"
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

TEST_F(HashTree, basic) {
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
