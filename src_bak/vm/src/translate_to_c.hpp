#ifndef HEADER_GUARD_NK_VM_TRANSLATE_TO_C
#define HEADER_GUARD_NK_VM_TRANSLATE_TO_C

#include <ostream>

#include "nk/vm/ir.hpp"

namespace nk {
namespace vm {

void translateToC(ir::Program const &ir, std::ostream &src);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_TRANSLATE_TO_C
