#ifndef HEADER_GUARD_NKB_IRC
#define HEADER_GUARD_NKB_IRC

#include "nk/common/string.h"
#include "nkb/common.h"

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

int nkir_compile(NkIrCompiler c, nkstr in_file, nkstr out_file, NkbOutputKind output_kind);
int nkir_run(NkIrCompiler c, nkstr in_file);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_IRC
