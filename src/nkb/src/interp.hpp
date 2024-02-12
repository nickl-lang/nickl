#ifndef NKB_INTERP_HPP_
#define NKB_INTERP_HPP_

#include "bytecode.hpp"

void nkir_interp_invoke(NkBcProc proc, void **args, void **ret);

#endif // NKB_INTERP_HPP_
