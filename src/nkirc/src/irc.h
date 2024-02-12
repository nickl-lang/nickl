#ifndef NKIRC_IRC_H_
#define NKIRC_IRC_H_

#include "nkb/ir.h"
#include "ntk/atom.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkIrCompiler_T *NkIrCompiler;

typedef struct {
    NkAtom libc_name;
    NkAtom libm_name;
    NkAtom libpthread_name;
    u8 ptr_size;
} NkIrcConfig;

NkIrCompiler nkirc_create(NkArena *tmp_arena, NkIrcConfig conf);
void nkirc_free(NkIrCompiler c);

int nkir_compile(NkIrCompiler c, NkString in_file, NkIrCompilerConfig conf);
int nkir_run(NkIrCompiler c, NkString in_file);

bool nkir_compileFile(NkIrCompiler c, NkString base_file, NkString in_file);

#ifdef __cplusplus
}
#endif

#endif // NKIRC_IRC_H_
