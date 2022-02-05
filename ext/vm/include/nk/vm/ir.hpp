#ifndef HEADER_GUARD_NK_VM_IR
#define HEADER_GUARD_NK_VM_IR

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "nk/common/arena.hpp"
#include "nk/common/array.hpp"
#include "nk/common/id.hpp"
#include "nk/vm/value.hpp"

namespace nk {
namespace vm {

#define REG_SIZE 8

enum ERegister {
    Reg_A,
    Reg_B,
    Reg_C,
    Reg_D,
    Reg_E,
    Reg_F,

    Reg_Count,
};

namespace ir {

// @Feature Implement atomic operations for thread synchronization

enum EIrCode {
#define X(NAME) ir_##NAME,
#include "nk/vm/ir.inl"

    Ir_Count,
};

enum ERefType {
    Ref_None,
    Ref_Frame,
    Ref_Arg,
    Ref_Ret,
    Ref_Global,
    Ref_Const,
    Ref_Reg,

    Ref_ExtVar,
};

struct Ref {
    union {
        void *data;
        size_t index;
    } value;
    size_t offset;
    size_t post_offset;
    type_t type;
    ERefType ref_type;
    bool is_indirect;

    Ref plus(size_t offset) const;
    Ref plus(size_t offset, type_t type) const;
    Ref deref(type_t type) const;
    Ref deref() const;
    Ref as(type_t type) const;
};

struct FunctId {
    size_t id;
};

struct BlockId {
    size_t id;
};

struct ShObjId {
    size_t id;
};

enum EArgType {
    Arg_None,

    Arg_Ref,
    Arg_BlockId,
    Arg_FunctId,
    Arg_ExtFunctId,
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
    size_t id;
    string name;

    type_t ret_t;
    type_t args_t;

    size_t first_block;
    size_t block_count;

    Array<type_t> locals;

    size_t next_block_id;
};

enum EExtSymType {
    Sym_Var,
    Sym_Funct,
};

struct ExSym {
    union {
        struct {
            type_t type;
        } var;
        struct {
            type_t ret_t;
            type_t args_t;
        } funct;
    } as;
    Id name;
    size_t so_id;
    EExtSymType sym_type;
};

struct Program {
    Array<Funct> functs;
    Array<Block> blocks;
    Array<Instr> instrs;
    Array<Id> shobjs;
    Array<ExSym> exsyms;

    size_t next_funct_id;

    Array<type_t> globals;

    ArenaAllocator arena;

    void init();
    void deinit();

    string inspect(Allocator &allocator);
};

struct Local {
    size_t id;
};

struct Global {
    size_t id;
};

struct ExtVarId {
    size_t id;
};

struct ExtFunctId {
    size_t id;
};

struct ProgramBuilder {
    Program *prog;

    void init(Program &prog);

    FunctId makeFunct();
    BlockId makeLabel();
    ShObjId makeShObj(Id name);

    //@Refactor Maybe IR functions don't need ids just like shobjs
    void startFunct(FunctId funct_id, string name, type_t ret_t, type_t args_t);
    void startBlock(BlockId block_id, string name);

    void comment(string str);

    Local makeLocalVar(type_t type);
    Global makeGlobalVar(type_t type);

    ExtVarId makeExtVar(ShObjId so, Id symbol, type_t type);
    ExtFunctId makeExtFunct(ShObjId so, Id symbol, type_t ret_t, type_t args_t);

    Ref makeFrameRef(Local var) const;
    Ref makeArgRef(size_t index) const;
    Ref makeRetRef() const;
    Ref makeGlobalRef(Global var) const;
    Ref makeConstRef(value_t val);
    Ref makeRegRef(ERegister reg, type_t type) const;
    Ref makeExtVarRef(ExtVarId var) const;

    template <class T>
    Ref makeConstRef(T &&val, type_t type) {
        assert(sizeof(val) == type->size);
        return makeConstRef({&val, type});
    }

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
    Instr make_call(Ref const &dst, ExtFunctId funct, Ref const &args);

#define U(NAME) Instr make_##NAME(Ref const &dst, Ref const &arg);
#include "nk/vm/ir.inl"

#define B(NAME) Instr make_##NAME(Ref const &dst, Ref const &lhs, Ref const &rhs);
#include "nk/vm/ir.inl"

    void gen(Instr const &instr);

private:
    Funct *m_cur_funct;
    Block *m_cur_block;
};

} // namespace ir
} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_IR
