#ifndef NK_VM_INTERP_HPP_
#define NK_VM_INTERP_HPP_

#include "bytecode.h"
#include "nk/vm/common.h"

void nk_interp_invoke(NkBcFunct fn, nkval_t ret, nkval_t args);

#endif // NK_VM_INTERP_HPP_
