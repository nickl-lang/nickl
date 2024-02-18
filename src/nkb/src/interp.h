#ifndef NKB_INTERP_H_
#define NKB_INTERP_H_

#include "bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkir_interp_invoke(NkBcProc proc, void **args, void **ret);

#ifdef __cplusplus
}
#endif

#endif // NKB_INTERP_H_
