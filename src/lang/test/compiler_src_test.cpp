#include <gtest/gtest.h>

#include "nkl/lang/compiler.h"
#include "nkl/lang/value.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/string.h"

namespace {

NK_LOG_USE_SCOPE(test);

class compiler_src : public testing::Test {
    void SetUp() override {
        NK_LOG_INIT({});

        char path_buf[NK_MAX_PATH];
        int path_len = nk_getBinaryPath(path_buf, sizeof(path_buf));
        if (path_len < 0) {
            FAIL() << "failed to get the compiler binary path";
        }

        m_compiler = nkl_compiler_create();
        nkl_compiler_configure(m_compiler, {path_buf, (usize)path_len});
    }

    void TearDown() override {
        nkl_compiler_free(m_compiler);

        nkl_types_clean();
    }

protected:
    void test(char const *src) {
        NK_LOG_INF("src:\n%s", src);
        nkl_compiler_runSrc(m_compiler, nk_cs2s(src));
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
