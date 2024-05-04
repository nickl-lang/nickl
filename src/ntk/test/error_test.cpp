#include "ntk/error.h"

#include <cstdio>

#include <gtest/gtest.h>

#include "ntk/arena.h"
#include "ntk/log.h"
#include "ntk/utils.h"

class Error : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});
    }

    void TearDown() override {
    }
};

TEST_F(Error, basic) {
    NkErrorState err{};
    nk_error_pushState(&err);
    defer {
        nk_error_freeState();
        nk_error_popState();
    };

    nk_error_printf("Hello, %s!", "Error");

    EXPECT_EQ(err.error_count, 1);

    auto errors = err.errors;

    EXPECT_NE(errors, nullptr);
    EXPECT_EQ(nk_s2stdStr(errors->msg), "Hello, Error!");
    errors = errors->next;

    EXPECT_EQ(errors, nullptr);
}

TEST_F(Error, nested) {
    NkErrorState err{};
    nk_error_pushState(&err);
    defer {
        nk_error_freeState();
        nk_error_popState();
    };

    nk_error_printf("Error 1");

    {
        NkErrorState err{};
        nk_error_pushState(&err);
        defer {
            nk_error_freeState();
            nk_error_popState();
        };

        nk_error_printf("Nested Error");

        EXPECT_EQ(err.error_count, 1);

        auto errors = err.errors;

        ASSERT_NE(errors, nullptr);
        EXPECT_EQ(nk_s2stdStr(errors->msg), "Nested Error");
        errors = errors->next;

        EXPECT_EQ(errors, nullptr);
    }

    nk_error_printf("Error 2");

    EXPECT_EQ(err.error_count, 2);

    auto errors = err.errors;

    ASSERT_NE(errors, nullptr);
    EXPECT_EQ(nk_s2stdStr(errors->msg), "Error 1");
    errors = errors->next;

    ASSERT_NE(errors, nullptr);
    EXPECT_EQ(nk_s2stdStr(errors->msg), "Error 2");
    errors = errors->next;

    EXPECT_EQ(errors, nullptr);
}

TEST_F(Error, arena) {
    NkArena arena{};

    NkErrorState err{};
    err.alloc = nk_arena_getAllocator(&arena);
    auto arena_frame = nk_arena_grab(&arena);
    nk_error_pushState(&err);
    defer {
        nk_error_popState();
        nk_arena_popFrame(&arena, arena_frame);
    };

    nk_error_printf("Hello, %s!", "Arena-stored Error");

    EXPECT_EQ(err.error_count, 1);

    auto errors = err.errors;

    EXPECT_NE(errors, nullptr);
    EXPECT_EQ(nk_s2stdStr(errors->msg), "Hello, Arena-stored Error!");
    errors = errors->next;

    EXPECT_EQ(errors, nullptr);
}
