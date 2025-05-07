#include "ntk/path.h"

#include <gtest/gtest.h>

#include "ntk/arena.h"
#include "ntk/string_builder.h"

static NkArena s_arena;

static NkString PATH(char const *path) {
    NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(&s_arena))};
    for (; *path; path++) {
        nksb_append(&sb, *path == '/' ? NK_PATH_SEPARATOR : *path);
    }
    nksb_appendNull(&sb);
    return (NkString){sb.data, sb.size - 1};
}

static NkString relativePath(NkString path, NkString base) {
    char *buf = (char *)nk_arena_alloc(&s_arena, NK_MAX_PATH);
    i32 len = nk_relativePath(buf, NK_MAX_PATH, path.data, base.data);
    nk_arena_pop(&s_arena, NK_MAX_PATH - len);
    return {buf, (usize)len - 1};
}

TEST(utils, relative_path) {
    EXPECT_EQ(relativePath(PATH(""), PATH("")), PATH(""));

    EXPECT_EQ(relativePath(PATH("/"), PATH("/")), PATH("."));
    EXPECT_EQ(relativePath(PATH("/one/two"), PATH("/one/two")), PATH("."));
    EXPECT_EQ(relativePath(PATH("/one/two"), PATH("/one")), PATH("two"));
    EXPECT_EQ(relativePath(PATH("/one/two/three"), PATH("/one")), PATH("two/three"));
    EXPECT_EQ(relativePath(PATH("/one"), PATH("/one/two")), PATH(".."));
    EXPECT_EQ(relativePath(PATH("/one"), PATH("/one/two/three")), PATH("../.."));
    EXPECT_EQ(relativePath(PATH("/one/two/three"), PATH("/one/four")), PATH("../two/three"));

    EXPECT_EQ(relativePath(PATH("C:/"), PATH("C:/")), PATH("."));
    EXPECT_EQ(relativePath(PATH("C:/one/two"), PATH("C:/one/two")), PATH("."));
    EXPECT_EQ(relativePath(PATH("C:/one/two"), PATH("C:/one")), PATH("two"));
    EXPECT_EQ(relativePath(PATH("C:/one/two/three"), PATH("C:/one")), PATH("two/three"));
    EXPECT_EQ(relativePath(PATH("C:/one"), PATH("C:/one/two")), PATH(".."));
    EXPECT_EQ(relativePath(PATH("C:/one"), PATH("C:/one/two/three")), PATH("../.."));
    EXPECT_EQ(relativePath(PATH("C:/one/two/three"), PATH("C:/one/four")), PATH("../two/three"));

    EXPECT_EQ(relativePath(PATH("C:/one"), PATH("D:/one")), PATH(""));
}

TEST(utils, parent) {
    EXPECT_EQ(nk_path_getParent(PATH("one/two/three")), PATH("one/two"));
    EXPECT_EQ(nk_path_getParent(PATH("one/two/")), PATH("one/two"));
    EXPECT_EQ(nk_path_getParent(PATH("one")), PATH(""));
    EXPECT_EQ(nk_path_getParent(PATH("")), PATH(""));
}

TEST(utils, filename) {
    EXPECT_EQ(nk_path_getFilename(PATH("one")), PATH("one"));
    EXPECT_EQ(nk_path_getFilename(PATH("one.two")), PATH("one.two"));
    EXPECT_EQ(nk_path_getFilename(PATH("one.two.three")), PATH("one.two.three"));

    EXPECT_EQ(nk_path_getFilename(PATH("zero/one")), PATH("one"));
    EXPECT_EQ(nk_path_getFilename(PATH("zero/one/two")), PATH("two"));
    EXPECT_EQ(nk_path_getFilename(PATH("zero/one/two.three")), PATH("two.three"));
}

TEST(utils, extension) {
    EXPECT_EQ(nk_path_getExtension(PATH("one")), PATH(""));
    EXPECT_EQ(nk_path_getExtension(PATH("one.two")), PATH("two"));
    EXPECT_EQ(nk_path_getExtension(PATH("one.two.three")), PATH("three"));

    EXPECT_EQ(nk_path_getExtension(PATH("zero/one")), PATH(""));
    EXPECT_EQ(nk_path_getExtension(PATH("zero/one.two")), PATH("two"));
    EXPECT_EQ(nk_path_getExtension(PATH("zero/one.two.three")), PATH("three"));

    EXPECT_EQ(nk_path_getExtension(PATH(".zero/one")), PATH(""));
    EXPECT_EQ(nk_path_getExtension(PATH(".zero/one.two")), PATH("two"));

    EXPECT_EQ(nk_path_getExtension(PATH(".git")), PATH("git"));
    EXPECT_EQ(nk_path_getExtension(PATH("repo/.git")), PATH("git"));

    EXPECT_EQ(nk_path_getExtension(PATH("..git")), PATH("git"));
    EXPECT_EQ(nk_path_getExtension(PATH("repo/..git")), PATH("git"));
}
