#include "nkl_core/id.hpp"

#include <gtest/gtest.h>

#include "nk_utils/logger.hpp"
#include "nk_utils/utils.hpp"

class id : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});
    }

    void TearDown() override {
    }
};

TEST_F(id, forward) {
    id_init();
    DEFER({ id_deinit(); })

    static constexpr char const *c_str_a = "String A.";
    static constexpr char const *c_str_b = "String B.";

    Id const id_a = cstr_to_id(c_str_a);
    Id const id_b = cstr_to_id(c_str_b);

    EXPECT_EQ(id_a, cstr_to_id(c_str_a));
    EXPECT_EQ(id_b, cstr_to_id(c_str_b));

    EXPECT_NE(id_a, id_b);
}

TEST_F(id, backward) {
    id_init();
    DEFER({ id_deinit(); })

    static constexpr char const *c_str_a = "This is a first string.";
    static constexpr char const *c_str_b = "This is a second string.";

    Id const id_a = cstr_to_id(c_str_a);
    Id const id_b = cstr_to_id(c_str_b);

    EXPECT_EQ(c_str_a, id_to_str(id_a).view());
    EXPECT_EQ(c_str_b, id_to_str(id_b).view());
}

TEST_F(id, nonexistent) {
    id_init();
    DEFER({ id_deinit(); })

    string str = id_to_str(9999999999);
    EXPECT_EQ(nullptr, str.data);
    EXPECT_EQ(0, str.size);
}
