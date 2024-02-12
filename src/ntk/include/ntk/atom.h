#ifndef NTK_ATOM_H_
#define NTK_ATOM_H_

#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32 NkAtom;

NkString nk_atom2s(NkAtom atom);
char const *nk_atom2cs(NkAtom atom);

NkAtom nk_s2atom(NkString str);
NkAtom nk_cs2atom(char const *str);

void nk_atom_define(NkAtom atom, NkString str);

enum {
    NK_ATOM_INVALID = 0,
};

#ifdef __cplusplus
}
#endif

#endif // NTK_ATOM_H_
