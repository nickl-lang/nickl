#ifndef HEADER_GUARD_NK_VM_INTERP
#define HEADER_GUARD_NK_VM_INTERP

#include "nk/vm/vm.hpp"

namespace nk {
namespace vm {

void interp_init(Program const *prog);

void interp_invoke(FunctInfo const&info, value_t ret, value_t args);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_INTERP
