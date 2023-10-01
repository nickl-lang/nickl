#include <algorithm>
#include <iterator>
#include <new>

#include <gtest/gtest.h>

#include "nkl/lang/ast.h"
#include "nkl/lang/compiler.h"
#include "nkl/lang/value.h"
#include "ntk/allocator.h"
#include "ntk/id.h"
#include "ntk/logger.h"
#include "ntk/string.h"
#include "ntk/sys/app.hpp"
#include "ntk/utils.hpp"

namespace {

NK_LOG_USE_SCOPE(test);

class compiler_src : public testing::Test {
    void SetUp() override {
        NK_LOGGER_INIT({});

        m_compiler = nkl_compiler_create();
        nkl_compiler_configure(m_compiler, nk_mkstr(nk_appDir().string().c_str()));
    }

    void TearDown() override {
        nkl_compiler_free(m_compiler);

        nkl_types_clean();
    }

protected:
    void test(char const *src) {
        NK_LOG_INF("src:\n%s", src);
        nkl_compiler_runSrc(m_compiler, nk_mkstr(src));
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
add :: (lhs: i64, rhs: i64) -> i64 {
    return lhs + rhs;
}
add(4, 5);
)");
}

TEST_F(compiler_src, comptime_const_getter) {
    test(R"(
counter := 0;
getVal :: () -> i64 {
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
#link("c")
puts :: (str: *i8) -> void;
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
import libc;
libc.puts("Hello, World!");
)");
}

TEST_F(compiler_src, comptime_declareLocal) {
    test(R"(
import libc;
import compiler;
${ compiler.declareLocal("str", *i8); }
str = "hello";
libc.puts(str);
)");
}
