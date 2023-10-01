#include "ntk/hash_set.hpp"

#include <gtest/gtest.h>

#include "ntk/logger.h"

class HashSet : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});
    }

    void TearDown() override {
    }
};

TEST_F(HashSet, basic) {
    using set_t = NkHashSet<int>;

    set_t set{};
    defer {
        set.deinit();
    };

    EXPECT_EQ(set.size(), 0);

    static constexpr int c_test_val = 42;

    auto found = set.find(c_test_val);
    EXPECT_EQ(found, nullptr);

    set.insert(c_test_val);

    EXPECT_EQ(set.size(), 1);

    found = set.find(c_test_val);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(*found, c_test_val);
}

TEST_F(HashSet, overwrite) {
    using set_t = NkHashSet<int>;

    set_t set{};
    defer {
        set.deinit();
    };

    static constexpr int c_test_val = 42;

    set.insert(c_test_val);
    EXPECT_EQ(set.size(), 1);

    set.insert(c_test_val);
    EXPECT_EQ(set.size(), 1);

    set.insert(c_test_val);
    EXPECT_EQ(set.size(), 1);
}

TEST_F(HashSet, iteration) {
    using set_t = NkHashSet<int>;

    set_t set{};
    defer {
        set.deinit();
    };

    set.insert(1);
    set.insert(1);
    set.insert(2);
    set.insert(3);
    set.insert(2);

    EXPECT_EQ(set.size(), 3);

    int sum = 0;
    for (auto &elem : (set_t const)set) {
        sum += elem;
    }
    EXPECT_EQ(sum, 6);
}

TEST_F(HashSet, allocator) {
    using set_t = NkHashSet<int>;

    NkArena arena{};
    auto set = set_t::create(nk_arena_getAllocator(&arena));
    defer {
        set.deinit();
        nk_arena_free(&arena);
    };

    static constexpr int c_test_val = 42;

    set.insert(c_test_val);

    EXPECT_EQ(set.size(), 1);

    auto found = set.find(c_test_val);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(*found, c_test_val);
}
