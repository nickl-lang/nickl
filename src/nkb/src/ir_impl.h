#ifndef NKB_IR_IMPL_H_
#define NKB_IR_IMPL_H_

#include "nkb/ir.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkAtom name;
    nktype_t type;
    usize offset;
} NkIrLocal_T;

typedef struct {
    NkDynArray(usize) blocks;
    NkDynArray(NkIrLocal_T) locals;
    NkDynArray(usize) scopes;

    NkAtom name;
    nktype_t proc_t;
    NkAtomArray arg_names;
    NkIrVisibility visibility;

    NkAtom file;
    usize start_line;
    usize end_line;

    usize cur_block;

    usize frame_size;
    usize frame_align;
    usize cur_frame_size;
} NkIrProc_T;

typedef struct {
    usize begin_idx;
    usize end_idx;
} NkIrInstrRange_T;

typedef struct {
    NkAtom name;
    NkDynArray(NkIrInstrRange_T) instr_ranges;
} NkIrBlock_T;

typedef struct NkIrRelocNode_T {
    struct NkIrRelocNode_T *next;

    NkIrRef address_ref;
    NkIrData target;
} NkIrRelocNode;

typedef struct {
    NkAtom name;
    NkIrVisibility visibility;
    void *data;
    nktype_t type;
    NkIrRelocNode *relocs;
    bool read_only;
} NkIrDecl_T;

typedef struct {
    NkAtom name;
    nktype_t type;
} NkIrExternSym_T;

struct NkIrModule_T {
    NkDynArray(usize) exported_procs;
    NkDynArray(usize) exported_data;
};

struct NkIrProg_T {
    NkAllocator alloc;

    NkDynArray(NkIrModule_T) modules;
    NkDynArray(NkIrProc_T) procs;
    NkDynArray(NkIrBlock_T) blocks;
    NkDynArray(NkIrInstr) instrs;
    NkDynArray(NkIrDecl_T) data;
    NkDynArray(NkIrExternSym_T) extern_data;
    NkDynArray(NkIrExternSym_T) extern_procs;

    NkIrProc cur_proc;
    u32 cur_line;

    NkString error_str;
};

#ifdef __cplusplus
}
#endif

#endif // NKB_IR_IMPL_H_
