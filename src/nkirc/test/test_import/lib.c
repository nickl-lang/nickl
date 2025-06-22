#include <stdint.h>

#define EXPORT __attribute__((__visibility__("default")))

EXPORT int64_t g_test_global_var;

EXPORT void setTestGlobalVar(int64_t val) {
    g_test_global_var = val;
}

EXPORT int64_t getTestGlobalVar() {
    return g_test_global_var;
}
