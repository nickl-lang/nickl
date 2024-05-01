#include "ntk/os/path.h"

#include <gtest/gtest.h>

#include "ntk/path.h"

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
    TEST_RELPATH("/one/two", "/one", "two");
    TEST_RELPATH("/one/two/three", "/one", "two/three");
    TEST_RELPATH("/one", "/one/two", "..");
    TEST_RELPATH("/one", "/one/two/three", "../..");
    TEST_RELPATH("/one/two/three", "/one/four", "../two/three");

    TEST_RELPATH("C:/", "C:/", ".");
    TEST_RELPATH("C:/one/two", "C:/one", "two");
    TEST_RELPATH("C:/one/two/three", "C:/one", "two/three");
    TEST_RELPATH("C:/one", "C:/one/two", "..");
    TEST_RELPATH("C:/one", "C:/one/two/three", "../..");
    TEST_RELPATH("C:/one/two/three", "C:/one/four", "../two/three");
}
