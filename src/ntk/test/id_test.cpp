#include "ntk/id.h"

#include <gtest/gtest.h>

#include "ntk/logger.h"
#include "ntk/string.h"
#include "ntk/utils.h"

class id : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});
    }

    void TearDown() override {
    }
};

TEST_F(id, forward) {
    static constexpr char const *c_str_a = "String A.";
    static constexpr char const *c_str_b = "String B.";

    nkid const id_a = cs2nkid(c_str_a);
    nkid const id_b = cs2nkid(c_str_b);

    EXPECT_EQ(id_a, cs2nkid(c_str_a));
    EXPECT_EQ(id_b, cs2nkid(c_str_b));

    EXPECT_NE(id_a, id_b);
}

TEST_F(id, backward) {
    static constexpr char const *c_str_a = "This is a first string.";
    static constexpr char const *c_str_b = "This is a second string.";

    nkid const id_a = cs2nkid(c_str_a);
    nkid const id_b = cs2nkid(c_str_b);

    EXPECT_EQ(c_str_a, std_view(nkid2s(id_a)));
    EXPECT_EQ(c_str_b, std_view(nkid2s(id_b)));
}

TEST_F(id, nonexistent) {
    auto str = nkid2s(999999999);
    EXPECT_EQ(nullptr, str.data);
    EXPECT_EQ(0, str.size);
}
