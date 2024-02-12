#ifndef NK_VM_IR_IMPL_HPP_
#define NK_VM_IR_IMPL_HPP_

#include <string>
#include <unordered_map>
#include <vector>

#include "bytecode.h"
#include "nk/vm/ir.h"
#include "nk/vm/value.h"

enum ENkIrFunctState {
    NkIrFunct_Incomplete,
    NkIrFunct_Complete,
};

struct NkIrFunct_T {
    NkIrProg prog;

    usize cur_block{};

    std::string name{};
    union {
        nktype_t fn_t;
        NktFnInfo fn_info;
    };
    ENkIrFunctState state{};

    std::vector<usize> blocks{};
    std::vector<nktype_t> locals{};

    NkBcFunct bc_funct{};
};

struct IrBlock {
    std::string name;

    std::vector<usize> instrs;
};

struct IrExSym {
    std::string name;
    NkIrShObjId so_id;
    nktype_t type;
};

struct NkIrProg_T {
    NkIrFunct cur_funct;

    std::vector<NkIrFunct> functs;
    std::vector<IrBlock> blocks;
    std::vector<NkIrInstr> instrs;
    std::vector<std::string> shobjs;
    std::vector<nktype_t> globals;
    std::vector<nkval_t> consts;
    std::vector<IrExSym> exsyms;

    NkBcProg bc;

    std::vector<NkIrNativeClosure> closures;
    std::unordered_map<void *, NkIrFunct> closureCode2IrFunct;
};

#endif // NK_VM_IR_IMPL_HPP_
