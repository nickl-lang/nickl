#ifndef HEADER_GUARD_NK_VM_C_COMPILER_STREAM
#define HEADER_GUARD_NK_VM_C_COMPILER_STREAM

#include <ostream>

#include "nk/common/string.hpp"

namespace nk {
namespace vm {

struct CCompilerConfig {
    string compiler_binary;
    string additional_flags;
    string output_filename;
    bool quiet;
};

std::ostream ccompiler_streamOpen(CCompilerConfig const &conf);
bool ccompiler_streamClose(std::ostream const &stream);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_C_COMPILER_STREAM
