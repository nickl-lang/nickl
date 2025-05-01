#include <stdint.h>

#include "ntk/common.h"

NK_EXPORT i64 g_test_global_var;

NK_EXPORT void setTestGlobalVar(i64 val) {
    g_test_global_var = val;
}

NK_EXPORT i64 getTestGlobalVar() {
    return g_test_global_var;
}
