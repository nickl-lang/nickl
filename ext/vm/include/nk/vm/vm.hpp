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

struct Program {
    Array<Instr> instrs;
    size_t globals_size;
    ArenaAllocator arena;
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
