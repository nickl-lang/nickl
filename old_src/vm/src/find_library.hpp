#ifndef HEADER_GUARD_NK_VM_FIND_LIBRARY
#define HEADER_GUARD_NK_VM_FIND_LIBRARY

#include "nk/str/string.hpp"

namespace nk {
namespace vm {

struct FindLibraryConfig {
    Slice<string> search_paths;
};

#define MAX_PATH 4096

void findLibrary_init(FindLibraryConfig const &conf);
void findLibrary_deinit();

bool findLibrary(string name, Slice<char> &buf);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_FIND_LIBRARY
