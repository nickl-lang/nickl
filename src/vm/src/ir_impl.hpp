#ifndef HEADER_GUARD_NK_VM_IR_IMPL
#define HEADER_GUARD_NK_VM_IR_IMPL

#include <cstddef>
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

    size_t cur_block{};

    std::string name{};
    union {
        nktype_t fn_t;
        NktFnInfo fn_info;
    };
    ENkIrFunctState state{};

    std::vector<size_t> blocks{};
    std::vector<nktype_t> locals{};

    NkBcFunct bc_funct{};
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

#endif // HEADER_GUARD_NK_VM_IR_IMPL
