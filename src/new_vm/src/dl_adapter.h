#ifndef HEADER_GUARD_NK_VM_DL_ADAPTER
#define HEADER_GUARD_NK_VM_DL_ADAPTER

#include "nk/common/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkDlHandle_T *NkDlHandle;

NkDlHandle nkdl_open(nkstr name);
void nkdl_close(NkDlHandle dl);

void *nkdl_sym(NkDlHandle dl, nkstr sym);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_DL_ADAPTER