#ifndef HEADER_GUARD_NK_VM_BC
#define HEADER_GUARD_NK_VM_BC

#include <cstddef>
#include <cstdint>

#include "nk/ds/array.hpp"
#include "nk/ds/log2arena.hpp"
#include "nk/mem/allocator.hpp"
#include "nk/vm/ir.hpp"
#include "nk/vm/value.hpp"

namespace nk {
namespace vm {
namespace bc {

extern char const *s_op_names[];

enum EOpCode {
#define X(NAME) op_##NAME,
#include "nk/vm/op.inl"

    Op_Count,
};

enum ERefType { // must preserve ir::ERefType order
    Ref_None,
    Ref_Frame,
    Ref_Arg,
    Ref_Ret,
    Ref_Global,
    Ref_Const,
    Ref_Reg,

    Ref_Instr,
    Ref_Abs,

    Ref_Count,
};

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
    size_t instr_count;
    type_t funct_t;
};

struct Program {
    Array<Instr> instrs;
    Array<uint8_t> globals;
    Array<uint8_t> rodata;
    Log2Arena<FunctInfo> funct_info;
    Array<void *> shobjs;

    void init();
    void deinit();

    string inspect(type_t fn, Allocator &allocator) const;
};

struct ProgramBuilder {
    ir::Program &ir_prog;
    Program &prog;

    type_t translate(ir::FunctId funct);
};

} // namespace bc
} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_BC
