#ifndef HEADER_GUARD_NK_VM_IR
#define HEADER_GUARD_NK_VM_IR

#include <cstddef>
#include <cstdint>

#include "nk/common/arena.hpp"
#include "nk/common/array.hpp"
#include "nk/vm/value.hpp"

namespace nk {
namespace vm {
namespace ir {

enum EIrCode {
#define X(NAME) ir_##NAME,
#include "nk/vm/ir.inl"
};

enum ERefType {
    Ref_None,

    Ref_Frame,
    Ref_Arg,
    Ref_Ret,
    Ref_Global,
    Ref_Const,

    Ref_Count,
};

struct Ref {
    union {
        void *data;
        size_t index;
    } value;
    size_t offset;
    type_t type;
    ERefType ref_type = Ref_None;
    bool is_indirect;
};

struct FunctId {
    size_t id;
};

struct BlockId {
    size_t id;
};

enum EArgType {
    Arg_None,

    Arg_Ref,
    Arg_BlockId,
    Arg_FunctId,
};

struct Arg {
    union {
        Ref ref;
        size_t id;
    } as;
    EArgType arg_type;
};

struct Instr {
    Arg arg[3];
    uint16_t code;
    string comment;
};

struct Block {
    size_t id;
    string name;

    size_t first_instr;
    size_t instr_count;

    string comment;
};

struct Funct {
    string name;

    type_t ret_t;
    type_t args_t;

    size_t first_block;
    size_t block_count;

    Array<type_t> locals;
};

struct Program {
    Array<Funct> functs;
    Array<Block> blocks;
    Array<Instr> instrs;

    Array<type_t> globals;

    ArenaAllocator arena;

    string inspect(Allocator &allocator) const;
};

struct Local {
    size_t id;
};

struct Global {
    size_t id;
};

struct ProgramBuilder {
    Program prog;

    void init();
    void deinit();

    FunctId makeFunct(string name, type_t ret_t, type_t args_t);
    BlockId makeLabel(string name);

    void startFunct(FunctId funct);
    void startBlock(BlockId label);

    void comment(string str);

    Local makeLocalVar(type_t type);
    Global makeGlobalVar(type_t type);

    Ref makeFrameRef(Local var);
    Ref makeArgRef(size_t index);
    Ref makeRetRef();
    Ref makeGlobalRef(Global var);
    Ref makeConstRef(value_t val);

    Ref refIndirect(Ref const &ref);
    Ref refOffset(Ref const &ref, size_t offset);

    Instr make_nop();
    Instr make_enter();
    Instr make_leave();
    Instr make_ret();

    Instr make_jmp(BlockId label);
    Instr make_jmpz(Ref const &cond, BlockId label);
    Instr make_jmpnz(Ref const &cond, BlockId label);

    Instr make_cast(Ref const &dst, Ref const &type, Ref const &arg);

    Instr make_call(Ref const &dst, FunctId funct, Ref const &args);
    Instr make_call(Ref const &dst, Ref const &funct, Ref const &args);

#define U(NAME) Instr make_##NAME(Ref const &dst, Ref const &arg);
#include "nk/vm/ir.inl"

#define B(NAME) Instr make_##NAME(Ref const &dst, Ref const &lhs, Ref const &rhs);
#include "nk/vm/ir.inl"

    void gen(Instr const &instr);

    string inspect(Allocator *allocator);

private:
    Arg _arg(Ref const &ref);
    Arg _arg(BlockId block);
    Arg _arg(FunctId funct);

    Funct *m_cur_funct;
    Block *m_cur_block;
};

} // namespace ir
} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_IR
