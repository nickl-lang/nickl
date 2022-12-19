#ifndef HEADER_GUARD_NK_VM_INTERP
#define HEADER_GUARD_NK_VM_INTERP

#include "nk/vm/common.h"
#include "op_internal.hpp"

void nk_interp_invoke(FunctInfo const &fn, nkval_t ret, nkval_t args);

#endif // HEADER_GUARD_NK_VM_INTERP
