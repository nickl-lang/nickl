#ifndef NKB_COMMON_H_
#define NKB_COMMON_H_

#include "nkb/ir.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkAtom name;
    u32 idx;
} Label;

typedef NkSlice(Label) LabelArray;
typedef NkDynArray(Label) LabelDynArray;

LabelArray collectLabels(NkIrInstrArray instrs, LabelDynArray *out);
u32 *countLabels(LabelArray labels);

Label const *findLabelByName(LabelArray labels, NkAtom name);
Label const *findLabelByIdx(LabelArray labels, u32 idx);

#ifdef __cplusplus
}
#endif

#endif // NKB_COMMON_H_
