#ifndef HEADER_GUARD_NK_VM_INTERP
#define HEADER_GUARD_NK_VM_INTERP

#include "bytecode_impl.hpp"
#include "nk/vm/common.h"

void nk_interp_invoke(BytecodeFunct const &fn, nkval_t ret, nkval_t args);

#endif // HEADER_GUARD_NK_VM_INTERP
