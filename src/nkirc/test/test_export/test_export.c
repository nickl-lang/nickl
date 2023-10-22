#include <stdint.h>
#include <stdio.h>

extern int64_t g_test_global_var;
extern void setTestGlobalVar(int64_t val);
extern int64_t getTestGlobalVar();

int main() {
    setTestGlobalVar(26);
    printf("val=%zi\n", g_test_global_var);
    return 0;
}

/* @output
val=26

@endoutput */
