#ifndef HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER
#define HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER

#include <ostream>

#include "nk/common/string.hpp"
#include "nk/vm/ir.h"

struct CCompilerConfig {
    nkstr compiler_binary;
    nkstr additional_flags;
    nkstr output_filename;
    bool quiet;
};

std::ostream nkcc_streamOpen(CCompilerConfig const &conf);
bool nkcc_streamClose(std::ostream const &stream);

bool nkcc_compile(CCompilerConfig const &conf, NkIrProg ir);

#endif // HEADER_GUARD_NK_VM_C_COMPILER_ADAPTER
