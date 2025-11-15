#include "llvm_emitter.h"

#include <float.h>
#include <inttypes.h>

#include "common.h"
#include "nkb/ir.h"
#include "nkb/types.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(llvm_emitter);

static void emitTypeEx(NkStream out, NkIrType type, usize base_offset, NkIrRelocArray relocs) {
    if (!type) {
        return;
    }

    switch (type->kind) {
        case NkIrType_Aggregate:
            if (type->size) {
                nk_print(out, "{");
                NK_ITERATE(NkIrAggregateElemInfo const *, elem, type->aggr) {
                    if (NK_INDEX(elem, type->aggr)) {
                        nk_print(out, ", ");
                    }
                    usize const offset = base_offset + elem->offset;
                    bool const is_string = elem->type->kind == NkIrType_Numeric && elem->type->size == 1;
                    if (elem->count > 1 || is_string) {
                        nk_printf(out, "[%u x ", elem->count);
                    }
                    bool found_reloc = false;
                    NK_ITERATE(NkIrReloc const *, reloc, relocs) {
                        if (reloc->offset == offset && elem->type->kind == NkIrType_Pointer) {
                            nk_print(out, "ptr");
                            found_reloc = true;
                            break;
                        }
                    }
                    if (!found_reloc) {
                        emitTypeEx(out, elem->type, offset, relocs);
                    }
                    if (elem->count > 1 || is_string) {
                        nk_print(out, "]");
                    }
                }
                nk_print(out, "}");
            } else {
                nk_print(out, "void");
            }

            break;

        case NkIrType_Numeric:
            switch (type->num) {
                case Int8:
                case Uint8:
                    nk_print(out, "i8");
                    break;
                case Int16:
                case Uint16:
                    nk_print(out, "i16");
                    break;
                case Int32:
                case Uint32:
                    nk_print(out, "i32");
                    break;
                case Int64:
                case Uint64:
                    nk_print(out, "i64");
                    break;
                case Float32:
                    nk_print(out, "float");
                    break;
                case Float64:
                    nk_print(out, "double");
                    break;
            }
            break;

        case NkIrType_Pointer:
            nk_print(out, "ptr");
            break;
    }
}

static void emitType(NkStream out, NkIrType type) {
    emitTypeEx(out, type, 0, (NkIrRelocArray){0});
}

static void emitVisibility(NkStream out, NkIrVisibility vis) {
    switch (vis) {
        case NkIrVisibility_Hidden:
            nk_print(out, "hidden");
            break;
        case NkIrVisibility_Default:
            nk_print(out, "dso_local");
            break;
        case NkIrVisibility_Protected:
            nk_print(out, "protected");
            break;
        case NkIrVisibility_Internal:
            nk_print(out, "hidden");
            break;
        case NkIrVisibility_Local:
            nk_print(out, "internal");
            break;
        case NkIrVisibility_Unknown:
            nk_assert(!"unreachable");
            break;
    }
}

static void emitGlobal(NkStream out, NkAtom name) {
    nk_print(out, "@");
    nkir_printSymbolName(out, name);
}

static void emitLocal(NkStream out, NkAtom name) {
    nk_print(out, "%");
    nkir_printSymbolName(out, name);
}

static void emitFloat(NkStream out, void *addr, NkIrNumericValueType value_type) {
    switch (value_type) {
        case Float32: {
            f32 const val = *(f32 *)addr;
            printFloat64Exact(out, (f64)val); // LLVM doens't support 32bit hex float constants
            break;
        }

        case Float64: {
            f64 const val = *(f64 *)addr;
            printFloat64Exact(out, val);
            break;
        }

        default:
            nk_assert(!"unreachable");
            break;
    }
}

static void emitRefUntyped(NkStream out, NkIrRef const *ref) {
    switch (ref->kind) {
        case NkIrRef_None:
            break;

        case NkIrRef_Null:
            break;

        case NkIrRef_Local:
        case NkIrRef_Param:
            emitLocal(out, ref->sym);
            break;

        case NkIrRef_Global:
            emitGlobal(out, ref->sym);
            break;

        case NkIrRef_Imm: {
            void *addr = (void *)&ref->imm;
            if (ref->type->kind == NkIrType_Pointer) {
                switch (ref->type->size) {
                    case 4: {
                        i32 val = *(i32 *)addr;
                        nk_assert(!val && "pointer constant can only be null");
                        break;
                    }

                    case 8: {
                        i64 val = *(i64 *)addr;
                        nk_assert(!val && "pointer constant can only be null");
                        break;
                    }

                    default:
                        nk_assert(!"pointer must be 4 or 8 bytes");
                        break;
                }
                nk_print(out, "zeroinitializer");
            } else {
                if (NKIR_NUMERIC_IS_INT(ref->type->num)) {
                    nkir_inspectVal(out, addr, ref->type);
                } else {
                    emitFloat(out, addr, ref->type->num);
                }
            }
            break;
        }

        case NkIrRef_VariadicMarker:
            nk_print(out, "...");
            break;
    }
}

static void emitRefType(NkStream out, NkIrRef const *ref) {
    switch (ref->kind) {
        case NkIrRef_None:
            break;

        case NkIrRef_Imm:
        case NkIrRef_Null:
        case NkIrRef_Local:
        case NkIrRef_Param:
        case NkIrRef_Global:
            emitType(out, ref->type);
            break;

        case NkIrRef_VariadicMarker:
            nk_print(out, "...");
            break;
    }
}

static void emitRef(NkStream out, NkIrRef const *ref) {
    emitRefType(out, ref);
    nk_print(out, " ");
    emitRefUntyped(out, ref);
}

typedef struct {
    NkArena *scratch;
    NkIrInstrArray instrs;

    LabelArray labels;
    u32 *indices;

    NkIrParamArray params;
    NkIrParam ret;

    usize next_local;
    usize next_label;
} Context;

static void emitLabel(Context *ctx, NkStream out, NkIrInstr const *instr, usize arg_idx) {
    NkIrArg const *arg = &instr->arg[arg_idx];
    usize const instr_idx = NK_INDEX(instr, ctx->instrs);

    nk_assert(arg->kind == NkIrArg_Label || arg->kind == NkIrArg_LabelRel);

    Label const *label = NULL;

    switch (arg->kind) {
        case NkIrArg_Label:
            label = ctx->instrs.data[instr_idx].code == NkIrOp_label ? findLabelByIdx(ctx->labels, instr_idx)
                                                                     : findLabelByName(ctx->labels, arg->label);
            break;

        case NkIrArg_LabelRel: {
            usize const target_idx = instr_idx + arg->offset;
            label = findLabelByIdx(ctx->labels, target_idx);
            break;
        }

        default:
            nk_assert(!"unreachable");
            break;
    }

    nk_assert(label && "invalid label");

    u32 const label_idx = ctx->indices[NK_INDEX(label, ctx->labels)];
    if (label_idx) {
        nk_printf(out, "%s%u", nk_atom2cs(label->name), label_idx);
    } else {
        nk_printf(out, "%s", nk_atom2cs(label->name));
    }
}

typedef enum {
    Prefix_Sign = 1 << 0,
} PrefixMask;

static void emitBinop(NkStream out, NkIrInstr const *instr, char const *name, PrefixMask mask) {
    NkIrRef const *ref0 = &instr->arg[0].ref;
    NkIrRef const *ref1 = &instr->arg[1].ref;
    NkIrRef const *ref2 = &instr->arg[2].ref;

    NkIrType const type = ref1->type;

    nk_assert(type->id == ref0->type->id);
    nk_assert(type->id == ref2->type->id);
    nk_assert(type->kind == NkIrType_Numeric);

    char const *prefix = "";

    if (NKIR_NUMERIC_IS_FLT(type->num)) {
        prefix = "f";
    } else if (mask & Prefix_Sign) {
        if (NKIR_NUMERIC_IS_SIGNED(type->num)) {
            prefix = "s";
        } else {
            prefix = "u";
        }
    }

    emitRefUntyped(out, ref0);
    nk_printf(out, " = %s%s ", prefix, name);
    emitType(out, type);
    nk_print(out, " ");
    emitRefUntyped(out, ref1);
    nk_print(out, ", ");
    emitRefUntyped(out, ref2);
}

static void emitLogic(NkStream out, NkIrInstr const *instr, char const *name, PrefixMask mask) {
    NkIrRef const *ref0 = &instr->arg[0].ref;
    NkIrRef const *ref1 = &instr->arg[1].ref;
    NkIrRef const *ref2 = &instr->arg[2].ref;

    NkIrType const type = ref1->type;

    nk_assert(type->id == ref2->type->id);
    nk_assert(type->kind == NkIrType_Numeric);

    char const *opcode_prefix = "";

    if ((mask & Prefix_Sign)) {
        if (NKIR_NUMERIC_IS_SIGNED(type->num)) {
            opcode_prefix = "a";
        } else {
            opcode_prefix = "l";
        }
    }

    emitRefUntyped(out, ref0);
    nk_printf(out, " = %s%s ", opcode_prefix, name);
    emitType(out, type);
    nk_print(out, " ");
    emitRefUntyped(out, ref1);
    nk_print(out, ", ");
    emitRefUntyped(out, ref2);
}

static void emitCondJmp(Context *ctx, NkStream out, NkIrInstr const *instr, char const *cond) {
    NkIrRef const *ref1 = &instr->arg[1].ref;

    usize const label = ctx->next_label++;
    usize const reg = ctx->next_local++;

    NkIrType const type = ref1->type;
    nk_assert(type->kind == NkIrType_Numeric);

    char const *opcode_prefix = NKIR_NUMERIC_IS_INT(type->num) ? "i" : "f";
    char const *cond_prefix = NKIR_NUMERIC_IS_INT(type->num) ? "" : "o";
    char const *fp_suffix = NKIR_NUMERIC_IS_INT(type->num) ? "" : ".0";

    nk_printf(out, "%%.%zu = %scmp %s%s ", reg, opcode_prefix, cond_prefix, cond);
    emitRef(out, ref1);
    nk_printf(out, ", 0%s\n  br i1 %%.%zu, label %%", fp_suffix, reg);
    emitLabel(ctx, out, instr, 2);
    nk_printf(out, ", label %%.label%zu\n.label%zu:", label, label);
}

static void emitCond(Context *ctx, NkStream out, NkIrInstr const *instr, char const *cond, PrefixMask mask) {
    NkIrRef const *ref0 = &instr->arg[0].ref;
    NkIrRef const *ref1 = &instr->arg[1].ref;
    NkIrRef const *ref2 = &instr->arg[2].ref;

    usize const reg = ctx->next_local++;

    NkIrType const type = ref1->type;
    NkIrType const dst_type = ref0->type;

    nk_assert(type->kind == NkIrType_Numeric);

    nk_assert(dst_type->kind == NkIrType_Numeric);
    nk_assert(NKIR_NUMERIC_IS_INT(dst_type->num));

    char const *opcode_prefix = NKIR_NUMERIC_IS_INT(type->num) ? "i" : "f";
    char const *ext_prefix = NKIR_NUMERIC_IS_SIGNED(dst_type->num) ? "s" : "z";
    char const *cond_prefix = NKIR_NUMERIC_IS_INT(type->num) ? "" : "o";

    if ((mask & Prefix_Sign) && NKIR_NUMERIC_IS_INT(type->num)) {
        if (NKIR_NUMERIC_IS_SIGNED(type->num)) {
            cond_prefix = "s";
        } else {
            cond_prefix = "u";
        }
    }

    nk_printf(out, "%%.%zu = %scmp %s%s ", reg, opcode_prefix, cond_prefix, cond);
    emitRef(out, ref1);
    nk_print(out, ", ");
    emitRefUntyped(out, ref2);
    nk_print(out, "\n  ");
    emitRefUntyped(out, ref0);
    nk_printf(out, " = %sext i1 %%.%zu to ", ext_prefix, reg);
    emitRefType(out, ref0);
}

static void emitMov(NkStream out, NkIrInstr const *instr) {
    NkIrRef const *ref0 = &instr->arg[0].ref;
    NkIrRef const *ref1 = &instr->arg[1].ref;

    emitRefUntyped(out, ref0);
    nk_print(out, " = bitcast ");
    emitType(out, ref1->type);
    nk_print(out, " ");
    emitRefUntyped(out, ref1);
    nk_print(out, " to ");
    emitRefType(out, ref0);
}

static void emitCast(NkStream out, NkIrInstr const *instr) {
    NkIrRef const *ref0 = &instr->arg[0].ref;
    NkIrRef const *ref1 = &instr->arg[1].ref;

    NkIrType const src_t = ref1->type;
    NkIrType const dst_t = ref0->type;
    nk_assert(src_t->kind == NkIrType_Numeric);
    nk_assert(dst_t->kind == NkIrType_Numeric);

    bool const src_is_int = NKIR_NUMERIC_IS_INT(src_t->num);
    bool const dst_is_int = NKIR_NUMERIC_IS_INT(dst_t->num);
    bool const src_is_signed = NKIR_NUMERIC_IS_SIGNED(src_t->num);
    bool const dst_is_signed = NKIR_NUMERIC_IS_SIGNED(dst_t->num);

    char const *op = "";
    char const *src = "";
    char const *dst = "";

    if (src_is_int == dst_is_int) {
        if (src_t->size == dst_t->size) {
            op = "bitcast";
        } else if (src_t->size < dst_t->size) {
            op = "ext";
            src = src_is_int ? (src_is_signed ? "s" : "z") : "fp";
        } else {
            op = "trunc";
            src = src_is_int ? "" : "fp";
        }
    } else {
        op = "to";
        src = src_is_int ? (src_is_signed ? "si" : "ui") : "fp";
        dst = dst_is_int ? (dst_is_signed ? "si" : "ui") : "fp";
    }

    emitRefUntyped(out, ref0);
    nk_printf(out, " = %s%s%s ", src, op, dst);
    emitType(out, ref1->type);
    nk_print(out, " ");
    emitRefUntyped(out, ref1);
    nk_print(out, " to ");
    emitRefType(out, ref0);
}

static void emitInstr(Context *ctx, NkStream out, NkIrInstr const *instr) {
    if (instr->code == NkIrOp_nop) {
        return;
    }

    if (instr->code != NkIrOp_label) {
        nk_print(out, "  ");
    }

    NkIrRef const *ref0 = &instr->arg[0].ref;
    NkIrRef const *ref1 = &instr->arg[1].ref;

    switch ((NkIrOpcode)instr->code) {
        case NkIrOp_nop:
            break;

        case NkIrOp_mov:
            emitMov(out, instr);
            break;

        case NkIrOp_cast:
            emitCast(out, instr);
            break;

        case NkIrOp_comment:
            nk_printf(out, "; " NKS_FMT, NKS_ARG(instr->arg[1].str));
            break;

        case NkIrOp_alloc: {
            emitRefUntyped(out, ref0);
            nk_print(out, " = alloca ");
            emitType(out, instr->arg[1].type);
            break;
        }

        case NkIrOp_offset: {
            emitRefUntyped(out, ref0);
            nk_print(out, " = getelementptr inbounds ");
            emitRefType(out, ref1);
            nk_print(out, ", ptr ");
            emitRefUntyped(out, ref1);
            nk_assert(ref1->type->kind == NkIrType_Aggregate);
            if (ref1->type->aggr.size == 1 && ref1->type->aggr.data[0].count > 1) {
                nk_print(out, ", i32 0, i32 0, ");
                emitRef(out, &instr->arg[2].ref);
            } else {
                nk_print(out, ", i32 0, ");
                emitRef(out, &instr->arg[2].ref);
            }
            break;
        }

        case NkIrOp_load: {
            emitRefUntyped(out, ref0);
            nk_print(out, " = load ");
            emitRefType(out, ref0);
            nk_print(out, ", ptr ");
            emitRefUntyped(out, ref1);
            break;
        }

        case NkIrOp_store: {
            nk_print(out, "store ");
            emitRef(out, ref1);
            nk_print(out, ", ptr ");
            emitRefUntyped(out, ref0);
            break;
        }

        case NkIrOp_jmp:
            nk_print(out, "br label %");
            emitLabel(ctx, out, instr, 1);
            break;

        case NkIrOp_jmpz:
            emitCondJmp(ctx, out, instr, "eq");
            break;

        case NkIrOp_jmpnz:
            emitCondJmp(ctx, out, instr, "ne");
            break;

        case NkIrOp_cmp_eq:
            emitCond(ctx, out, instr, "eq", 0);
            break;
        case NkIrOp_cmp_ne:
            emitCond(ctx, out, instr, "ne", 0);
            break;
        case NkIrOp_cmp_gt:
            emitCond(ctx, out, instr, "gt", Prefix_Sign);
            break;
        case NkIrOp_cmp_ge:
            emitCond(ctx, out, instr, "ge", Prefix_Sign);
            break;
        case NkIrOp_cmp_lt:
            emitCond(ctx, out, instr, "lt", Prefix_Sign);
            break;
        case NkIrOp_cmp_le:
            emitCond(ctx, out, instr, "le", Prefix_Sign);
            break;

        case NkIrOp_label:
            emitLabel(ctx, out, instr, 1);
            nk_print(out, ":");
            break;

        case NkIrOp_add:
            emitBinop(out, instr, "add", 0);
            break;
        case NkIrOp_sub:
            emitBinop(out, instr, "sub", 0);
            break;
        case NkIrOp_mul:
            emitBinop(out, instr, "mul", 0);
            break;

        case NkIrOp_div:
            emitBinop(out, instr, "div", Prefix_Sign);
            break;
        case NkIrOp_mod:
            emitBinop(out, instr, "rem", Prefix_Sign);
            break;

        case NkIrOp_and:
            emitLogic(out, instr, "and", 0);
            break;
        case NkIrOp_or:
            emitLogic(out, instr, "or", 0);
            break;
        case NkIrOp_xor:
            emitLogic(out, instr, "xor", 0);
            break;
        case NkIrOp_lsh:
            emitLogic(out, instr, "shl", 0);
            break;
        case NkIrOp_rsh:
            emitLogic(out, instr, "shr", Prefix_Sign);
            break;

        case NkIrOp_call: {
            NkIrRefArray const arg_refs = instr->arg[2].refs;

            bool sret = false;
            if (ref0->kind && ref0->kind != NkIrRef_Null) {
                if (ref0->type->kind == NkIrType_Aggregate && ref0->type->size) {
                    sret = true;
                } else {
                    emitRefUntyped(out, ref0);
                    nk_print(out, " = ");
                }
            }

            nk_print(out, "call ");
            if (sret || !ref0->kind) {
                nk_print(out, "void");
            } else {
                emitType(out, ref0->type);
            }

            nk_print(out, " (");
            if (sret) {
                nk_print(out, "ptr");
            }

            NK_ITERATE(NkIrRef const *, arg_ref, arg_refs) {
                if (NK_INDEX(arg_ref, arg_refs) || sret) {
                    nk_print(out, ", ");
                }
                if (arg_ref->type && arg_ref->type->kind == NkIrType_Aggregate) {
                    nk_print(out, "ptr");
                } else {
                    emitRefType(out, arg_ref);
                }
                if (arg_ref->kind == NkIrRef_VariadicMarker) {
                    break;
                }
            }

            nk_printf(out, ") ");
            emitRefUntyped(out, ref1);
            nk_printf(out, "(");

            if (sret) {
                nk_print(out, "ptr sret(");
                emitType(out, ref0->type);
                nk_printf(out, ") align %u ", ref0->type->align);
                emitRefUntyped(out, ref0);
            }

            NK_ITERATE(NkIrRef const *, arg_ref, arg_refs) {
                if (arg_ref->kind == NkIrRef_VariadicMarker) {
                    continue;
                }
                if (NK_INDEX(arg_ref, arg_refs) || sret) {
                    nk_print(out, ", ");
                }

                if (arg_ref->type->kind == NkIrType_Aggregate) {
                    nk_print(out, "ptr byval(");
                    emitRefType(out, arg_ref);
                    nk_printf(out, ") align %u", arg_ref->type->align);
                } else {
                    emitRefType(out, arg_ref);
                }
                nk_print(out, " ");
                emitRefUntyped(out, arg_ref);
            }

            nk_print(out, ")");

            break;
        }

        case NkIrOp_ret:
            nk_print(out, "ret ");
            if (ref1->kind) {
                emitRef(out, ref1);
            } else {
                nk_print(out, "void");
            }
            break;
    }

    nk_print(out, "\n");
}

static void emitVal(NkStream out, void *base_addr, usize base_offset, NkIrRelocArray relocs, NkIrType type) {
    nk_assert(base_addr && "trying to inspect nullptr");

    emitTypeEx(out, type, base_offset, relocs);
    nk_print(out, " ");

    switch (type->kind) {
        case NkIrType_Aggregate:
            nk_print(out, "{");
            NK_ITERATE(NkIrAggregateElemInfo const *, elem, type->aggr) {
                if (NK_INDEX(elem, type->aggr)) {
                    nk_print(out, ", ");
                }
                usize offset = base_offset + elem->offset;
                bool const is_string = elem->type->kind == NkIrType_Numeric && elem->type->size == 1;
                if (elem->count > 1 || is_string) {
                    nk_printf(out, "[%u x ", elem->count);
                    emitTypeEx(out, elem->type, offset, relocs);
                    nk_print(out, "] ");
                }
                if (is_string) {
                    char const *addr = (char *)base_addr + offset;
                    nk_print(out, "c\"");
                    nks_sanitize(out, (NkString){addr, type->aggr.data[0].count});
                    nk_print(out, "\"");
                } else {
                    if (elem->count > 1) {
                        nk_print(out, "[");
                    }
                    for (usize i = 0; i < elem->count; i++) {
                        if (i) {
                            nk_print(out, ", ");
                        }
                        bool found_reloc = false;
                        NK_ITERATE(NkIrReloc const *, reloc, relocs) {
                            if (reloc->offset == offset && elem->type->kind == NkIrType_Pointer) {
                                nk_print(out, "ptr ");
                                emitGlobal(out, reloc->sym);
                                found_reloc = true;
                                break;
                            }
                        }
                        if (!found_reloc) {
                            emitVal(out, base_addr, offset, relocs, elem->type);
                        }
                        offset += elem->type->size;
                    }
                    if (elem->count > 1) {
                        nk_print(out, "]");
                    }
                }
            }
            nk_print(out, "}");
            break;

        case NkIrType_Numeric: {
            void *addr = (u8 *)base_addr + base_offset;
            if (NKIR_NUMERIC_IS_INT(type->num)) {
                nkir_inspectVal(out, addr, type);
            } else {
                emitFloat(out, addr, type->num);
            }
            break;
        }

        case NkIrType_Pointer:
            nk_assert(!"cannot write pointer constant");
            break;
    }
}

static void emitData(NkStream out, NkIrData const *data) {
    nk_printf(out, "%s ", (data->flags & NkIrData_ReadOnly) ? "constant" : "global");

    if (data->addr) {
        emitVal(out, data->addr, 0, data->relocs, data->type);
    } else {
        switch (data->type->kind) {
            case NkIrType_Aggregate:
            case NkIrType_Pointer:
                emitType(out, data->type);
                nk_print(out, " zeroinitializer");
                break;

            case NkIrType_Numeric: {
                NkIrImm imm;
                memset(&imm, 0, sizeof(imm));
                emitVal(out, &imm, 0, (NkIrRelocArray){0}, data->type);
                break;
            }
        }
    }

    nk_print(out, "\n");
}

static void emitSymbol(NkStream out, NkArena *scratch, NkIrSymbol const *sym) {
    switch (sym->kind) {
        case NkIrSymbol_None:
            break;

        case NkIrSymbol_Proc: {
            LabelDynArray da_labels = {.alloc = nk_arena_getAllocator(scratch)};
            LabelArray const labels = collectLabels(sym->proc.instrs, &da_labels);

            u32 *indices = countLabels(scratch, labels);

            Context ctx = {
                .scratch = scratch,
                .instrs = sym->proc.instrs,

                .labels = labels,
                .indices = indices,

                .params = sym->proc.params,
                .ret = sym->proc.ret,
            };

            nk_print(out, "define ");
            emitVisibility(out, sym->vis);
            nk_print(out, " ");
            if (ctx.ret.name) {
                nk_print(out, "void");
            } else {
                emitType(out, sym->proc.ret.type);
            }
            nk_print(out, " ");
            emitGlobal(out, sym->name);
            nk_print(out, "(");
            if (ctx.ret.name) {
                nk_print(out, "ptr sret(");
                emitType(out, ctx.ret.type);
                nk_printf(out, ") align %u ", ctx.ret.type->align);
                emitLocal(out, ctx.ret.name);
            }
            NK_ITERATE(NkIrParam const *, param, ctx.params) {
                if (NK_INDEX(param, sym->proc.params) || ctx.ret.name) {
                    nk_print(out, ", ");
                }
                if (param->type->kind == NkIrType_Aggregate) {
                    nk_print(out, "ptr byval(");
                }
                emitType(out, param->type);
                if (param->type->kind == NkIrType_Aggregate) {
                    nk_printf(out, ") align %u", param->type->align);
                }
                nk_print(out, " ");
                emitLocal(out, param->name);
            }
            nk_print(out, ") {\n");
            NK_ITERATE(NkIrInstr const *, instr, sym->proc.instrs) {
                emitInstr(&ctx, out, instr);
            }
            nk_print(out, "}\n");
            break;
        }

        case NkIrSymbol_Data:
            emitGlobal(out, sym->name);
            nk_print(out, " = ");
            emitVisibility(out, sym->vis);
            nk_print(out, " ");
            emitData(out, &sym->data);
            break;

        case NkIrSymbol_Extern:
            switch (sym->extrn.kind) {
                case NkIrExtern_Proc: {
                    bool const sret =
                        sym->extrn.proc.ret_type->kind == NkIrType_Aggregate && sym->extrn.proc.ret_type->size;
                    nk_print(out, "declare ");
                    if (sret) {
                        nk_print(out, "void");
                    } else {
                        emitType(out, sym->extrn.proc.ret_type);
                    }
                    nk_print(out, " ");
                    emitGlobal(out, sym->name);
                    nk_print(out, "(");
                    if (sret) {
                        nk_print(out, "ptr sret(");
                        emitType(out, sym->extrn.proc.ret_type);
                        nk_printf(out, ") align %u", sym->extrn.proc.ret_type->align);
                    }
                    NK_ITERATE(NkIrType const *, type, sym->extrn.proc.param_types) {
                        if (NK_INDEX(type, sym->extrn.proc.param_types) || sret) {
                            nk_print(out, ", ");
                        }
                        if ((*type)->kind == NkIrType_Aggregate) {
                            nk_print(out, "ptr byval(");
                        }
                        emitType(out, *type);
                        if ((*type)->kind == NkIrType_Aggregate) {
                            nk_printf(out, ") align %u", (*type)->align);
                        }
                    }
                    if (sym->extrn.proc.flags & NkIrProc_Variadic) {
                        nk_print(out, ", ...");
                    }
                    nk_print(out, ")\n");
                    break;
                }

                case NkIrExtern_Data:
                    emitGlobal(out, sym->name);
                    nk_print(out, " = external global ");
                    emitType(out, sym->extrn.data.type);
                    nk_print(out, "\n");
                    break;
            }
            break;
    }

    nk_print(out, "\n");
}

void nk_llvm_emitIr(NkStream out, NkArena *scratch, NkIrSymbolArray mod) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC() {
        NK_ITERATE(NkIrSymbol const *, sym, mod) {
            emitSymbol(out, scratch, sym);
        }
    }
}
