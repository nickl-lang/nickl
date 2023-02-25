#ifndef HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER
#define HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER

#include <ostream>

#include "nk/common/string.hpp"
#include "nk/vm/ir_compile.h"

std::ostream nkcc_streamOpen(NkIrCompilerConfig const &conf);
bool nkcc_streamClose(std::ostream const &stream);

#endif // HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER
