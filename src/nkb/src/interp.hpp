#ifndef HEADER_GUARD_NKB_INTERP
#define HEADER_GUARD_NKB_INTERP

#include "bytecode.hpp"

void nkir_interp_invoke(NkBcProc proc, void **args, void **ret);

#endif // HEADER_GUARD_NKB_INTERP
