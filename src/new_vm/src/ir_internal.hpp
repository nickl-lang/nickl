#ifndef HEADER_GUARD_NK_VM_IR_INTERNAL
#define HEADER_GUARD_NK_VM_IR_INTERNAL

#include <cstddef>
#include <string>
#include <vector>

#include "nk/vm/ir.h"
#include "op.h"

#ifdef __cplusplus
extern "C" {
#endif

struct IrFunct {
    std::string name;
    nktype_t fn_t;

    std::vector<size_t> blocks;
    std::vector<nktype_t> locals;
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
    size_t cur_funct;
    size_t cur_block;

    std::vector<IrFunct> functs;
    std::vector<IrBlock> blocks;
    std::vector<NkIrInstr> instrs;
    std::vector<std::string> shobjs;
    std::vector<nktype_t> globals;
    std::vector<IrExSym> exsyms;

    NkOpProg op;
};

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_IR_INTERNAL
