#ifndef HEADER_GUARD_NK_VM_DL_ADAPTER
#define HEADER_GUARD_NK_VM_DL_ADAPTER

#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkDlHandle_T *NkDlHandle;

NkDlHandle nkdl_open(nks name);
void nkdl_close(NkDlHandle dl);

void *nkdl_sym(NkDlHandle dl, nks sym);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_DL_ADAPTER
