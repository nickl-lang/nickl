#include "ntk/string_builder.h"

#include <cinttypes>

#include <gtest/gtest.h>

#include "ntk/string.h"
#include "ntk/utils.h"

class string_builder : public testing::Test {
    void SetUp() override {
    }

    void TearDown() override {
    }

protected:
};

TEST_F(string_builder, basic) {
    NkStringBuilder sb{};
    defer {
        nksb_free(&sb);
    };

    nksb_printf(&sb, "Hello, %s!", "World");

    EXPECT_EQ(nk_s2stdView({NK_SLICE_INIT(sb)}), "Hello, World!");
}

TEST_F(string_builder, counting) {
    NkStringBuilder sb{};
    defer {
        nksb_free(&sb);
    };

    nksb_printf(&sb, "[");

    for (usize i = 0; i < 10; i++) {
        nksb_printf(&sb, "%" PRIu64, i);
    }

    nksb_printf(&sb, "]");

    EXPECT_EQ(nk_s2stdView({NK_SLICE_INIT(sb)}), "[0123456789]");
}

TEST_F(string_builder, stream) {
    NkStringBuilder sb{};
    defer {
        nksb_free(&sb);
    };

    auto stream = nksb_getStream(&sb);

    nk_stream_printf(stream, "Hello");
    nk_stream_printf(stream, ", %s!", "World");

    EXPECT_EQ(nk_s2stdView({NK_SLICE_INIT(sb)}), "Hello, World!");
}
