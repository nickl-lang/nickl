#include "nk/common/string_builder.h"

#include <cinttypes>

#include <gtest/gtest.h>

#include "nk/common/string.hpp"
#include "nk/common/utils.hpp"

class string_builder : public testing::Test {
    void SetUp() override {
    }

    void TearDown() override {
    }

protected:
};

TEST_F(string_builder, basic) {
    NkStringBuilder_T sb{};
    defer {
        nksb_free(&sb);
    };

    nksb_printf(&sb, "Hello, %s!", "World");
    nkstr str{sb.data, sb.size};

    EXPECT_EQ(std_view(str), "Hello, World!");
}

TEST_F(string_builder, counting) {
    NkStringBuilder_T sb{};
    defer {
        nksb_free(&sb);
    };

    nksb_printf(&sb, "[");

    for (size_t i = 0; i < 10; i++) {
        nksb_printf(&sb, "%" PRIu64, i);
    }

    nksb_printf(&sb, "]");

    nkstr str{sb.data, sb.size};

    EXPECT_EQ(std_view(str), "[0123456789]");
}
