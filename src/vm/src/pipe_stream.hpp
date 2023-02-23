#ifndef HEADER_GUARD_NK_VM_PIPE_STREAM
#define HEADER_GUARD_NK_VM_PIPE_STREAM

#include <istream>
#include <ostream>

#include "nk/common/string.hpp"

std::istream nk_pipe_streamRead(nkstr cmd, bool quiet = false);
std::ostream nk_pipe_streamWrite(nkstr cmd, bool quiet = false);
bool nk_pipe_streamClose(std::ios const &stream);

#endif // HEADER_GUARD_NK_VM_PIPE_STREAM
