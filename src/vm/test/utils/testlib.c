#include "testlib.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

bool s_test_printf_called;
int64_t test_val;

int test_printf(char const *fmt, ...) {
    s_test_printf_called = true;

    va_list ap;
    va_start(ap, fmt);
    int res = vprintf(fmt, ap);
    va_end(ap);
    return res;
}
