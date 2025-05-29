#include "ntk/string.h"

#include <gtest/gtest.h>

#include "ntk/arena.h"

class string : public testing::Test {
    void SetUp() override {
    }

    void TearDown() override {
        nk_arena_free(&m_arena);
    }

protected:
    NkArena m_arena{};
};

#define SHELL_LEX_TEST(STR, ...)                                \
    do {                                                        \
        auto const res = nks_shell_lex(&m_arena, nk_cs2s(STR)); \
        usize i = 0;                                            \
        for (char const *exp : {__VA_ARGS__}) {                 \
            ASSERT_LT(i, res.size);                             \
            EXPECT_STREQ(res.data[i].data, exp);                \
            i++;                                                \
        }                                                       \
        EXPECT_EQ(i, res.size);                                 \
    } while (0)

TEST_F(string, shell_lex) {
    SHELL_LEX_TEST("one two three", "one", "two", "three");
    SHELL_LEX_TEST("   one   two   three   ", "one", "two", "three");

    SHELL_LEX_TEST("one \"two\" three", "one", "two", "three");
    SHELL_LEX_TEST("one \"two   three\" four", "one", "two   three", "four");
    SHELL_LEX_TEST("one \"two three \\\"four\\\"\" five", "one", "two three \"four\"", "five");
    SHELL_LEX_TEST("one\\ two three", "one two", "three");

    SHELL_LEX_TEST("\"hello\"", "hello");
    SHELL_LEX_TEST("\"hello\" \"world\"", "hello", "world");
    SHELL_LEX_TEST("\"'hello'\" \"'world'\"", "'hello'", "'world'");
    SHELL_LEX_TEST("'\"hello\"' '\"world\"'", "\"hello\"", "\"world\"");
}
