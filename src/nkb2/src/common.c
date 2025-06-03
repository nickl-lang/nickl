#include "common.h"

#include "ntk/arena.h"

LabelArray collectLabels(NkIrInstrArray instrs, LabelDynArray *out) {
    NK_ITERATE(NkIrInstr const *, instr, instrs) {
        if (instr->code == NkIrOp_label) {
            nkda_append(
                out,
                ((Label){
                    .name = instr->arg[1].label,
                    .idx = NK_INDEX(instr, instrs),
                }));
        }
    }
    return (LabelArray){NK_SLICE_INIT(*out)};
}

u32 *countLabels(NkArena *arena, LabelArray labels) {
    u32 *indices = nk_arena_allocTn(arena, u32, labels.size);

    NK_ITERATE(Label const *, label1, labels) {
        u32 count = 0;
        for (usize j = 0; j < NK_INDEX(label1, labels); j++) {
            Label const *label2 = &labels.data[j];
            if (nks_equal(nk_atom2s(label1->name), nk_atom2s(label2->name))) {
                count++;
            }
        }
        indices[NK_INDEX(label1, labels)] = count;
    }

    return indices;
}

Label const *findLabelByName(LabelArray labels, NkAtom name) {
    NK_ITERATE(Label const *, label, labels) {
        if (label->name == name) {
            return label;
        }
    }
    return NULL;
}

Label const *findLabelByIdx(LabelArray labels, u32 idx) {
    NK_ITERATE(Label const *, label, labels) {
        if (label->idx == idx) {
            return label;
        }
    }
    return NULL;
}
