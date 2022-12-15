#include "nk/common/string_builder.h"

#include <gtest/gtest.h>

class string_builder : public testing::Test {
    void SetUp() override {
    }

    void TearDown() override {
    }

protected:
};

TEST_F(string_builder, basic) {
    auto sb = nksb_create();

    nksb_printf(sb, "Hello, %s!", "World");
    auto str = nksb_concat(sb);

    EXPECT_STREQ(str, "Hello, World!");

    nksb_free(sb);
}
