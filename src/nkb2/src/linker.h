#ifndef NKB_LINKER_H_
#define NKB_LINKER_H_

#include "nkb/ir.h"
#include "ntk/arena.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkArena *scratch;
    NkIrOutputKind out_kind;
    NkString obj_file;
    NkString out_file;
} NkLikerOpts;

bool nk_link(NkLikerOpts const opts);

#ifdef __cplusplus
}
#endif

#endif // NKB_LINKER_H_
