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
    ir::ERefType ref_type;
    bool is_indirect;
};

struct Instr {
    Ref arg[3];
    uint16_t code;
};

struct FunctInfo {
    type_t frame_t;
    size_t first_instr;
    type_t funct_t;
};

struct Program {
    Array<Instr> instrs;
    type_t globals_t;
    Array<uint8_t> rodata;
    Array<FunctInfo> funct_info;
};

struct Translator {
    Program prog;

    void init();
    void deinit();

    void translateFromIr(ir::Program const &ir);

    string inspect(Allocator *allocator);
};

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_VM
