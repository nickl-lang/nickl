#include "op.h"

#include <iostream>

namespace {} // namespace

struct NkOpProg_T {};

NkOpProg nkop_createProgram(NkIrProg ir) {
}

void nkop_deinitProgram(NkOpProg p) {
    if (p) {
    }
}

void nkop_invoke(NkOpProg p, NkIrFunctId fn, nkval_t ret, nkval_t args) {
}
