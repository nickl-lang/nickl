#ifndef NTK_ATOM_H_
#define NTK_ATOM_H_

#include "ntk/string.h"
#include "ntk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32 NkAtom;

NK_EXPORT void nk_atom_init(void);
NK_EXPORT void nk_atom_deinit(void);

NK_EXPORT NkString nk_atom2s(NkAtom atom);
NK_EXPORT char const *nk_atom2cs(NkAtom atom);

NK_EXPORT NkAtom nk_s2atom(NkString str);
NK_EXPORT NkAtom nk_cs2atom(char const *str);

NK_EXPORT void nk_atom_define(NkAtom atom, NkString str);

NK_EXPORT NkAtom nk_atom_unique(NkString str);

NK_INLINE u64 nk_atom_hash(NkAtom atom) {
    return nk_hashVal(atom);
}

NK_INLINE bool nk_atom_equal(NkAtom lhs, NkAtom rhs) {
    return lhs == rhs;
}

#ifdef __cplusplus
}
#endif

#endif // NTK_ATOM_H_
