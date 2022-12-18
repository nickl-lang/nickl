#ifndef HEADER_GUARD_NK_VM_OP
#define HEADER_GUARD_NK_VM_OP

#include <stddef.h>
#include <stdint.h>

#include "nk/common/utils.h"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define X(NAME) CAT(nkop_, NAME),
#include "op.inl"

    nkop_count,
} NkOpCode;

extern char const *s_op_names[];

typedef enum { // must preserve NkIrRefType order
    NkOpRef_None,
    NkOpRef_Frame,
    NkOpRef_Arg,
    NkOpRef_Ret,
    NkOpRef_Global,
    NkOpRef_Const,
    NkOpRef_Reg,

    NkOpRef_Instr,
    NkOpRef_Abs,

    NkOpRef_Count,
} NkOpRefType;

typedef struct {
    size_t offset;
    size_t post_offset;
    nktype_t type;
    NkOpRefType ref_type;
    bool is_indirect;
} NkOpRef;

typedef struct {
    NkOpRef arg[3];
    uint16_t code;
} NkOpInstr;

typedef struct NkOpProg_T *NkOpProg;

NkOpProg nkop_createProgram(NkIrProg ir);
void nkop_deinitProgram(NkOpProg p);

void nkop_invoke(NkOpProg p, NkIrFunctId fn, nkval_t ret, nkval_t args);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_VM_OP
