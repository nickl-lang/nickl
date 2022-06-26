#include "nk/common/id.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.h"
#include "nk/common/utils.hpp"

using namespace nk;

class id : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(id, forward) {
    id_init();
    defer {
        id_deinit();
    };

    static constexpr char const *c_str_a = "String A.";
    static constexpr char const *c_str_b = "String B.";

    Id const id_a = cs2id(c_str_a);
    Id const id_b = cs2id(c_str_b);

    EXPECT_EQ(id_a, cs2id(c_str_a));
    EXPECT_EQ(id_b, cs2id(c_str_b));

    EXPECT_NE(id_a, id_b);
}

TEST_F(id, backward) {
    id_init();
    defer {
        id_deinit();
    };

    static constexpr char const *c_str_a = "This is a first string.";
    static constexpr char const *c_str_b = "This is a second string.";

    Id const id_a = cs2id(c_str_a);
    Id const id_b = cs2id(c_str_b);

    EXPECT_EQ(c_str_a, std_view(id2s(id_a)));
    EXPECT_EQ(c_str_b, std_view(id2s(id_b)));
}

TEST_F(id, nonexistent) {
    id_init();
    defer {
        id_deinit();
    };

    string str = id2s(9999999999);
    EXPECT_EQ(nullptr, str.data);
    EXPECT_EQ(0, str.size);
}
