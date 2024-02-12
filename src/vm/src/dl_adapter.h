#ifndef NK_VM_DL_ADAPTER_H_
#define NK_VM_DL_ADAPTER_H_

#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NkDlHandle_T *NkDlHandle;

NkDlHandle nkdl_open(NkString name);
void nkdl_close(NkDlHandle dl);

void *nkdl_sym(NkDlHandle dl, NkString sym);

#ifdef __cplusplus
}
#endif

#endif // NK_VM_DL_ADAPTER_H_
