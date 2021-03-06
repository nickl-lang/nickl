#ifndef HEADER_GUARD_NK_VM_INTERP
#define HEADER_GUARD_NK_VM_INTERP

#include "nk/vm/bc.hpp"

namespace nk {
namespace vm {

void interp_invoke(type_t self, value_t ret, value_t args);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_INTERP
