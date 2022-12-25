#ifndef HEADER_GUARD_NK_VM_IR_IMPL
#define HEADER_GUARD_NK_VM_IR_IMPL

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

#include "bytecode.h"
#include "nk/vm/ir.h"

struct NkIrFunct_T {
    NkIrProg prog;
    NkIrFunct self_ptr{};

    size_t cur_block{};

    std::string name{};
    nktype_t fn_t{};

    std::vector<size_t> blocks{};
    std::vector<nktype_t> locals{};
};

struct IrBlock {
    std::string name;

    std::vector<size_t> instrs;
};

struct IrExSym {
    std::string name;
    NkIrShObjId so_id;
    nktype_t type;
};

struct NkIrProg_T {
    NkIrFunct cur_funct;

    std::deque<NkIrFunct_T> functs;
    std::vector<IrBlock> blocks;
    std::vector<NkIrInstr> instrs;
    std::vector<std::string> shobjs;
    std::vector<nktype_t> globals;
    std::vector<IrExSym> exsyms;

    NkBcProg bc;
};

#endif // HEADER_GUARD_NK_VM_IR_IMPL