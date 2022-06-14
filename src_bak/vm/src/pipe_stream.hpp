#ifndef HEADER_GUARD_NK_VM_PIPE_STREAM
#define HEADER_GUARD_NK_VM_PIPE_STREAM

#include <istream>
#include <ostream>

#include "nk/common/string.hpp"

namespace nk {
namespace vm {

std::istream pipe_streamRead(string cmd, bool quiet = false);
std::ostream pipe_streamWrite(string cmd, bool quiet = false);
bool pipe_streamClose(std::ios const &stream);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_PIPE_STREAM
