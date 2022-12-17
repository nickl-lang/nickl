#include "nk/common/string_builder.h"

#include <gtest/gtest.h>

#include "nk/common/utils.hpp"

class string_builder : public testing::Test {
    void SetUp() override {
    }

    void TearDown() override {
    }

protected:
};

TEST_F(string_builder, basic) {
    auto sb = nksb_create();
    defer {
        nksb_free(sb);
    };

    nksb_printf(sb, "Hello, %s!", "World");
    auto str = nksb_concat(sb);

    EXPECT_STREQ(str, "Hello, World!");
}

TEST_F(string_builder, counting) {
    auto sb = nksb_create();
    defer {
        nksb_free(sb);
    };

    nksb_printf(sb, "[");

    for (size_t i = 0; i < 10; i++) {
        nksb_printf(sb, "%i", i);
    }

    nksb_printf(sb, "]");

    auto str = nksb_concat(sb);

    EXPECT_STREQ(str, "[0123456789]");
}
