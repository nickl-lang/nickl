#include "translate2c.h"

void nkir_translate2c(NkIrProg ir, NkIrProc entry_point, nk_stream src) {
    char code[] =
        "#include <stdio.h>                \n"
        "int main(int argc, char** argv) { \n"
        "    puts(\"Hello, World!\");      \n"
        "    return 0;                     \n"
        "}";
    nk_stream_write(src, code, sizeof(code) - 1);
}
