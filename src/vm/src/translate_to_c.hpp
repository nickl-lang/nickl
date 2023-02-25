#ifndef HEADER_GUARD_NK_VM_TRANSLATE_TO_C
#define HEADER_GUARD_NK_VM_TRANSLATE_TO_C

#include <ostream>

#include "nk/vm/ir.h"

void nkir_translateToC(NkIrProg ir, NkIrFunct entry_point, std::ostream &src);

#endif // HEADER_GUARD_NK_VM_TRANSLATE_TO_C
