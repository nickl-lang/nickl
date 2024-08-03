#ifndef NKL_CORE_COMPILER_API_H_
#define NKL_CORE_COMPILER_API_H_

#include "nkl/core/nickl.h"
#include "ntk/common.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_EXPORT StringSlice nickl_api_getCommandLineArgs(NklState nkl);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_COMPILER_API_H_
