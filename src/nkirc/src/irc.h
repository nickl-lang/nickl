#ifndef HEADER_GUARD_NKIRC_IRC
#define HEADER_GUARD_NKIRC_IRC

#include "nkb/ir.h"
#include "ntk/id.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkIrCompiler_T *NkIrCompiler;

typedef struct {
    nkid libc_name;
    nkid libm_name;
    nkid libpthread_name;
    uint8_t usize;
} NkIrcConfig;

NkIrCompiler nkirc_create(NkArena *tmp_arena, NkIrcConfig conf);
void nkirc_free(NkIrCompiler c);

int nkir_compile(NkIrCompiler c, nks in_file, NkIrCompilerConfig conf);
int nkir_run(NkIrCompiler c, nks in_file);

bool nkir_compileFile(NkIrCompiler c, nks base_file, nks in_file);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKIRC_IRC
