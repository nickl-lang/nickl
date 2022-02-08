#ifndef HEADER_GUARD_NK_VM_VM
#define HEADER_GUARD_NK_VM_VM

#include "nk/vm/common_types.hpp"

namespace nk {
namespace vm {

struct VmConfig {
    FindLibraryConfig find_library;
};

void vm_init(VmConfig const &conf);
void vm_deinit();

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_VM
