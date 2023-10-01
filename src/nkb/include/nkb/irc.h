#ifndef HEADER_GUARD_NKB_IRC
#define HEADER_GUARD_NKB_IRC

#include "nkb/common.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkIrCompiler_T *NkIrCompiler;

typedef enum {
    NkIrcColor_Auto,
    NkIrcColor_Always,
    NkIrcColor_Never,
} NkIrcColorPolicy;

typedef struct {
    NkIrcColorPolicy color_policy;
} NkIrcOptions;

NkIrCompiler nkirc_create(NkIrcOptions opts);
void nkirc_free(NkIrCompiler c);

int nkir_compile(NkIrCompiler c, nks in_file, nks out_file, NkbOutputKind output_kind);
int nkir_run(NkIrCompiler c, nks in_file);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_IRC
