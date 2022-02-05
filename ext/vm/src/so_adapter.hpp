#ifndef HEADER_GUARD_NK_VM_SO_ADAPTER
#define HEADER_GUARD_NK_VM_SO_ADAPTER

#include "nk/common/string.hpp"

namespace nk {
namespace vm {

void *openSharedObject(string name);
void closeSharedObject(void *handle);
void *resolveSym(void *so, string sym);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_SO_ADAPTER
