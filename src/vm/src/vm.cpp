#include "nk/vm/vm.hpp"

#include "find_library.hpp"
#include "nk/common/id.hpp"
#include "nk/vm/value.hpp"

namespace nk {
namespace vm {

void vm_init(VmConfig const &conf) {
    id_init();
    types_init();
    findLibrary_init(conf.find_library);
}

void vm_deinit() {
    findLibrary_deinit();
    types_deinit();
    id_deinit();
}

} // namespace vm
} // namespace nk
