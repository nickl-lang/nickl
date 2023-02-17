#include <algorithm>
#include <iterator>
#include <new>

#include <gtest/gtest.h>

#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/string.h"
#include "nk/common/utils.hpp"
#include "nkl/lang/ast.h"
#include "nkl/lang/compiler.h"

namespace {

NK_LOG_USE_SCOPE(test);

class compiler_src : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT(NkLoggerOptions{});

        m_compiler = nkl_compiler_create({}); // TODO Providing empty stdlib dir
    }

    void TearDown() override {
        nkl_compiler_free(m_compiler);
    }

protected:
    void test(char const *src) {
        NK_LOG_INF("src:\n%s", src);
        nkl_compiler_runSrc(m_compiler, cs2s(src));
    }

protected:
    NklCompiler m_compiler;
};

} // namespace

TEST_F(compiler_src, empty) {
    test(R"(
)");
}

TEST_F(compiler_src, basic) {
    test(R"(
2 + 2;
)");
}

TEST_F(compiler_src, comptime_const) {
    test(R"(
pi :: 3.14;
)");
}

TEST_F(compiler_src, fn) {
    test(R"(
add :: (lhs: u32, rhs: u32) -> u32 {
    return lhs + rhs;
}
add(4, 5);
)");
}

TEST_F(compiler_src, comptime_const_getter) {
    test(R"(
counter := 0;
getVal :: () -> u32 {
    counter = counter + 1;
    return 42;
}
val :: getVal();
val;
val;
)");
}

TEST_F(compiler_src, native_puts) {
    test(R"(
#foreign("")
puts :: (str: *u8) -> void;
puts("Hello, World!");
)");
}

TEST_F(compiler_src, fast_exp) {
    test(R"(
b := 2;
n := 16;
a := 1;
c := b;
while n != 0 {
    r := n % 2;
    if r == 1 {
        a = a * c;
    }
    n = n / 2;
    c = c * c;
}
)");
}

TEST_F(compiler_src, import) {
    test(R"(
import std;
std.puts("Hello, World!");
)");
}

TEST_F(compiler_src, comptime_declareLocal) {
    test(R"(
import std;
import compiler;
${ compiler.declareLocal("str", *u8); }
str = "hello";
std.puts(str);
)");
}
