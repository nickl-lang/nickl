#include <stdint.h>

#include "ntk/common.h"

NK_EXPORT int64_t g_test_global_var;

NK_EXPORT void setTestGlobalVar(int64_t val) {
    g_test_global_var = val;
}

NK_EXPORT int64_t getTestGlobalVar() {
    return g_test_global_var;
}
