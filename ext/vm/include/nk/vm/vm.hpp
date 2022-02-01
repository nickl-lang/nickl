#ifndef HEADER_GUARD_NK_VM_VM
#define HEADER_GUARD_NK_VM_VM

#include <cstddef>
#include <cstdint>

#include "nk/common/arena.hpp"
#include "nk/common/array.hpp"
#include "nk/vm/ir.hpp"
#include "nk/vm/value.hpp"

namespace nk {
namespace vm {

struct Ref {
    size_t offset;
    size_t post_offset;
    type_t type;
    ERefType ref_type;
    bool is_indirect;
};

struct Instr {
    Ref arg[3];
    uint16_t code;
};

struct Program;

struct FunctInfo {
    Program *prog;
    type_t frame_t;
    size_t first_instr;
    type_t funct_t;
};

struct Program {
    Array<Instr> instrs;
    type_t globals_t;
    uint8_t *globals;
    Array<uint8_t> rodata;
    Array<FunctInfo> funct_info;

    void deinit();

    string inspect(Allocator *allocator);
};

struct Translator {
    type_t translateFromIr(Program &prog, ir::Program const &ir);
};

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_VM
