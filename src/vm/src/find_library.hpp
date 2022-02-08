#ifndef HEADER_GUARD_NK_VM_FIND_LIBRARY
#define HEADER_GUARD_NK_VM_FIND_LIBRARY

#include "nk/vm/common_types.hpp"

namespace nk {
namespace vm {

#define MAX_PATH 4096

void findLibrary_init(FindLibraryConfig const &conf);
void findLibrary_deinit();

bool findLibrary(string name, Slice<char> &buf);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_FIND_LIBRARY
