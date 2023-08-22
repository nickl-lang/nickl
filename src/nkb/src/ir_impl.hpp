#ifndef HEADER_GUARD_NKB_IR_IMPL
#define HEADER_GUARD_NKB_IR_IMPL

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

// #include "bytecode.h"
#include "nkb/ir.h"
// #include "nkb/value.h"

enum ENkIrProcState {
    NkIrProc_Incomplete,
    NkIrProc_Complete,
};

// struct NkIrProc_T {
//     NkIrProg ir;

//     size_t cur_block{};

//     std::string name{};
//     union {
//         nktype_t proc_t;
//         NkProcTypeInfo proc_info;
//     };
//     ENkIrProcState state{};

//     std::vector<size_t> blocks{};
//     std::vector<nktype_t> locals{};

//     // NkBcProc bc_proc{};
// };

struct NkIrBlock {
    std::string name;

    std::vector<size_t> instrs;
};

// struct IrExSym {
//     std::string name;
//     NkIrShObjId so_id;
//     nktype_t type;
// };

struct NkIrProg_T {
    NkAllocator alloc;

    NkIrProc cur_proc;

    std::vector<NkIrProc> procs;
    std::vector<NkIrBlock> blocks;
    std::vector<NkIrInstr> instrs;
    std::vector<std::string> shobjs;
    std::vector<nktype_t> globals;
    // std::vector<nkval_t> consts;
    // std::vector<IrExSym> exsyms;

    // NkBc bc;

    std::unordered_map<void *, NkIrProc> closureCode2IrProc;
};

#endif // HEADER_GUARD_NKB_IR_IMPL
