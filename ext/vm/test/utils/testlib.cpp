#include "testlib.hpp"

#include <cstdarg>
#include <cstdio>

extern "C" {

bool s_test_printf_called;
int64_t s_global_val;

int test_printf(char const *fmt, ...) {
    s_test_printf_called = 1;

    va_list ap;
    va_start(ap, fmt);
    auto res = vprintf(fmt, ap);
    va_end(ap);
    return res;
}

} // extern
