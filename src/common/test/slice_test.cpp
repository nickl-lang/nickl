#include "nk/common/slice.hpp"

#include <array>
#include <iterator>
#include <vector>

#include <gtest/gtest.h>

#include "nk/common/logger.h"

class slice : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST(slice, iterate) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    Slice<int> s{v.data(), v.size()};
    int sum = 0;
    int prev = 0;
    for (auto x : s) {
        EXPECT_GT(x, prev);
        prev = x;
        sum += x;
    }
    EXPECT_EQ(sum, 15);
}

TEST(slice, iterate_reverse) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    Slice<int> s{v.data(), v.size()};
    int sum = 0;
    int prev = 6;
    for (auto it = std::rbegin(s); it != std::rend(s); ++it) {
        EXPECT_LT(*it, prev);
        prev = *it;
        sum += *it;
    }
    EXPECT_EQ(sum, 15);
}

TEST(slice, access) {
    std::vector<int> v = {1, 2, 3, 4, 5};
    Slice<int> s{v.data(), v.size()};
    EXPECT_EQ(s.front(), 1);
    EXPECT_EQ(s.back(), 5);
    EXPECT_EQ(s[1], 2);
    EXPECT_EQ(s.data, v.data());
    EXPECT_EQ(s.size, 5);
}

TEST(slice, empty) {
    Slice<int> s{};
    EXPECT_EQ(s.data, nullptr);
    EXPECT_EQ(s.size, 0);
}

TEST(slice, const) {
    std::vector<int> const cvec{4, 3, 2, 1, 0};
    Slice<int const> cs{cvec.data(), cvec.size()};
    EXPECT_EQ(cs.data, cvec.data());
    EXPECT_EQ(cs.size, cvec.size());
}
