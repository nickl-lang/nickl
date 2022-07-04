#ifndef HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER
#define HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER

#include <ostream>

#include "nk/str/string.hpp"

namespace nk {
namespace vm {

struct CCompilerConfig {
    string compiler_binary;
    string additional_flags;
    string output_filename;
    bool quiet;
};

std::ostream c_compiler_streamOpen(CCompilerConfig const &conf);
bool c_compiler_streamClose(std::ostream const &stream);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER
