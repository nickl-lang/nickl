#ifndef HEADER_GUARD_NKB_NKB
#define HEADER_GUARD_NKB_NKB

#include "nk/common/string.h"
#include "nkb/common.h"

#ifdef __cplusplus
extern "C" {
#endif

void nkb_compile(nkstr in_file, nkstr out_file, NkbOutputKind output_kind);
void nkb_run(nkstr in_file);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKB_NKB
