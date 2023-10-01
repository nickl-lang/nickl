#ifndef HEADER_GUARD_NK_VM_IR_COMPILE
#define HEADER_GUARD_NK_VM_IR_COMPILE

#include "nk/vm/ir.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nkstr compiler_binary;
    nkstr additional_flags;
    nkstr output_filename;
    bool quiet;
} NkIrCompilerConfig;

bool nkir_compile(NkIrCompilerConfig conf, NkIrProg ir, NkIrFunct entry_point);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_IR_COMPILE
