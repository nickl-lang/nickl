#include "nk/ds/hashset.hpp"

#include <gtest/gtest.h>

#include "nk/utils/logger.h"

using namespace nk;

class hashset : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(hashset, basic) {
    using set_t = HashSet<int>;

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

TEST_F(hashset, overwrite) {
    using set_t = HashSet<int>;

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
