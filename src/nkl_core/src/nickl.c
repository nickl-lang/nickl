#include "nkl/core/nickl.h"

#include "nodes.h"
#include "ntk/atom.h"

void nkl_state_init(NklState *nkl) {
#define XN(N, T) nk_atom_define(NK_CAT(n_, N), nk_cs2s(T));
#include "nodes.inl"
}

void nkl_state_free(NklState *nkl) {
}
