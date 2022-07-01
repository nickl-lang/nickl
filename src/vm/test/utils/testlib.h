#ifndef HEADER_GUARD_NK_VM_TEST_TESTLIB
#define HEADER_GUARD_NK_VM_TEST_TESTLIB

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool s_test_printf_called;

int test_printf(char const *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_TEST_TESTLIB
