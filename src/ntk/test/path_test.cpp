#include "ntk/path.h"

#include <gtest/gtest.h>

// TODO: Simplify this test impl

static void fixPath(char *buf, char const *path) {
    usize i = 0;
    for (; *path; path++, i++) {
        buf[i] = *path == '/' ? NK_PATH_SEPARATOR : *path;
    }
    buf[i] = '\0';
}

#define TEST_RELPATH(path, base, expected)                         \
    {                                                              \
        char path_fixed[NK_MAX_PATH]{};                            \
        fixPath(path_fixed, path);                                 \
                                                                   \
        char base_fixed[NK_MAX_PATH]{};                            \
        fixPath(base_fixed, base);                                 \
                                                                   \
        char expected_fixed[NK_MAX_PATH]{};                        \
        fixPath(expected_fixed, expected);                         \
                                                                   \
        char buf[NK_MAX_PATH]{};                                   \
        nk_relativePath(buf, sizeof(buf), path_fixed, base_fixed); \
        EXPECT_STREQ(buf, expected_fixed);                         \
    }

TEST(utils, relative_path) {
    TEST_RELPATH("", "", "");

    TEST_RELPATH("/", "/", ".");
    TEST_RELPATH("/one/two", "/one/two", ".");
    TEST_RELPATH("/one/two", "/one", "two");
    TEST_RELPATH("/one/two/three", "/one", "two/three");
    TEST_RELPATH("/one", "/one/two", "..");
    TEST_RELPATH("/one", "/one/two/three", "../..");
    TEST_RELPATH("/one/two/three", "/one/four", "../two/three");

    TEST_RELPATH("C:/", "C:/", ".");
    TEST_RELPATH("C:/one/two", "C:/one/two", ".");
    TEST_RELPATH("C:/one/two", "C:/one", "two");
    TEST_RELPATH("C:/one/two/three", "C:/one", "two/three");
    TEST_RELPATH("C:/one", "C:/one/two", "..");
    TEST_RELPATH("C:/one", "C:/one/two/three", "../..");
    TEST_RELPATH("C:/one/two/three", "C:/one/four", "../two/three");

    TEST_RELPATH("C:/one", "D:/one", "");
}

#define TEST_PARENT(path, expected)                              \
    {                                                            \
        char path_fixed[NK_MAX_PATH]{};                          \
        fixPath(path_fixed, path);                               \
                                                                 \
        char expected_fixed[NK_MAX_PATH]{};                      \
        fixPath(expected_fixed, expected);                       \
                                                                 \
        auto const res = nk_path_getParent(nk_cs2s(path_fixed)); \
        EXPECT_EQ(res, nk_cs2s(expected_fixed));                 \
    }

TEST(utils, parent) {
    TEST_PARENT("one/two/three", "one/two");
    TEST_PARENT("one/two/", "one/two");
    TEST_PARENT("one", "");
    TEST_PARENT("", "");
}

#define TEST_FILENAME(path, expected)                              \
    {                                                              \
        char path_fixed[NK_MAX_PATH]{};                            \
        fixPath(path_fixed, path);                                 \
                                                                   \
        char expected_fixed[NK_MAX_PATH]{};                        \
        fixPath(expected_fixed, expected);                         \
                                                                   \
        auto const res = nk_path_getFilename(nk_cs2s(path_fixed)); \
        EXPECT_EQ(res, nk_cs2s(expected_fixed));                   \
    }

TEST(utils, filename) {
    TEST_FILENAME("one", "one");
    TEST_FILENAME("one.two", "one.two");
    TEST_FILENAME("one.two.three", "one.two.three");

    TEST_FILENAME("zero/one", "one");
    TEST_FILENAME("zero/one/two", "two");
    TEST_FILENAME("zero/one/two.three", "two.three");
}

#define TEST_EXTENSION(path, expected)                              \
    {                                                               \
        char path_fixed[NK_MAX_PATH]{};                             \
        fixPath(path_fixed, path);                                  \
                                                                    \
        char expected_fixed[NK_MAX_PATH]{};                         \
        fixPath(expected_fixed, expected);                          \
                                                                    \
        auto const res = nk_path_getExtension(nk_cs2s(path_fixed)); \
        EXPECT_EQ(res, nk_cs2s(expected_fixed));                    \
    }

TEST(utils, extension) {
    TEST_EXTENSION("one", "");
    TEST_EXTENSION("one.two", "two");
    TEST_EXTENSION("one.two.three", "three");

    TEST_EXTENSION("zero/one", "");
    TEST_EXTENSION("zero/one.two", "two");
    TEST_EXTENSION("zero/one.two.three", "three");

    TEST_EXTENSION(".zero/one", "");
    TEST_EXTENSION(".zero/one.two", "two");

    TEST_EXTENSION(".git", "git");
    TEST_EXTENSION("repo/.git", "git");

    TEST_EXTENSION("..git", "git");
    TEST_EXTENSION("repo/..git", "git");
}
