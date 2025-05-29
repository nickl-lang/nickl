#ifndef NK_VM_IR_COMPILE_H_
#define NK_VM_IR_COMPILE_H_

#include "nk/vm/ir.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkString compiler_binary;
    NkString additional_flags;
    NkString output_filename;
    bool quiet;
} NkIrCompilerConfig;

bool nkir_compile(NkArena *scratch, NkIrCompilerConfig const conf, NkIrProg ir, NkIrFunct entry_point);

#ifdef __cplusplus
}
#endif

#endif // NK_VM_IR_COMPILE_H_
