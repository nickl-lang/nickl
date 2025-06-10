#include "emit_llvm.h"

#include <float.h>
#include <inttypes.h>

#include "common.h"
#include "nkb/ir.h"
#include "nkb/types.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"
// #include "ntk/log.h"

// NK_LOG_USE_SCOPE(emit_llvm);

static void emitTypeEx(NkStream out, NkIrType type, usize base_offset, NkIrRelocArray relocs) {
    if (!type) {
        return;
    }

    switch (type->kind) {
        case NkIrType_Aggregate:
            if (type->size) {
                nk_printf(out, "{");
                NK_ITERATE(NkIrAggregateElemInfo const *, elem, type->aggr) {
                    if (NK_INDEX(elem, type->aggr)) {
                        nk_printf(out, ", ");
                    }
                    usize const offset = base_offset + elem->offset;
                    if (elem->count > 1) {
                        nk_printf(out, "[%u x ", elem->count);
                    }
                    bool found_reloc = false;
                    NK_ITERATE(NkIrReloc const *, reloc, relocs) {
                        if (reloc->offset == offset && elem->type->size == 8) { // TODO: Hardcoded ptr size
                            nk_printf(out, "ptr");
                            found_reloc = true;
                            break;
                        }
                    }
                    if (!found_reloc) {
                        emitTypeEx(out, elem->type, offset, relocs);
                    }
                    if (elem->count > 1) {
                        nk_printf(out, "]");
                    }
                }
                nk_printf(out, "}");
            } else {
                nk_printf(out, "void");
            }

            break;

        case NkIrType_Numeric:
            switch (type->num) {
                case Int8:
                case Uint8:
                    nk_printf(out, "i8");
                    break;
                case Int16:
                case Uint16:
                    nk_printf(out, "i16");
                    break;
                case Int32:
                case Uint32:
                    nk_printf(out, "i32");
                    break;
                case Int64:
                case Uint64:
                    nk_printf(out, "i64");
                    break;
                case Float32:
                    nk_printf(out, "float");
                    break;
                case Float64:
                    nk_printf(out, "double");
                    break;
            }
            break;
    }
}

static void emitType(NkStream out, NkIrType type) {
    emitTypeEx(out, type, 0, (NkIrRelocArray){0});
}

static void emitVisibility(NkStream out, NkIrVisibility vis) {
    switch (vis) {
        case NkIrVisibility_Hidden:
            nk_printf(out, "hidden");
            break;
        case NkIrVisibility_Default:
            nk_printf(out, "dso_local");
            break;
        case NkIrVisibility_Protected:
            nk_printf(out, "protected");
            break;
        case NkIrVisibility_Internal:
            nk_printf(out, "hidden");
            break;
        case NkIrVisibility_Local:
            nk_printf(out, "internal");
            break;
    }
}

static void emitName(NkStream out, NkAtom name) {
    NkString const name_str = nk_atom2s(name);
    if (name_str.size) {
        nk_printf(out, NKS_FMT, NKS_ARG(name_str));
    } else {
        nk_printf(out, "_%u", name);
    }
}

static void emitGlobal(NkStream out, NkAtom name) {
    nk_printf(out, "@");
    emitName(out, name);
}

static void emitLocal(NkStream out, NkAtom name) {
    nk_printf(out, "%%");
    emitName(out, name);
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
            if (NKIR_NUMERIC_IS_INT(ref->type->num)) {
                nkir_inspectVal(addr, ref->type, out);
            } else {
                emitFloat(out, addr, ref->type->num);
            }
            break;
        }

        case NkIrRef_VariadicMarker:
            nk_printf(out, "...");
            break;
    }
}

static void emitRefType(NkStream out, NkIrRef const *ref) {
    switch (ref->kind) {
        case NkIrRef_None:
            break;

        case NkIrRef_Global:
            nk_printf(out, "ptr");
            break;

        case NkIrRef_Imm:
        case NkIrRef_Null:
        case NkIrRef_Local:
        case NkIrRef_Param:
            emitType(out, ref->type);
            break;

        case NkIrRef_VariadicMarker:
            nk_printf(out, "...");
            break;
    }
}

static void emitRef(NkStream out, NkIrRef const *ref) {
    emitRefType(out, ref);
    nk_printf(out, " ");
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
} ProcContext;

typedef struct {
    NkArena *scratch;
    ProcContext proc;
} Context;

static bool refIsPtr(Context *ctx, NkIrRef const *ref) {
    bool is_ptr = ref->kind == NkIrRef_Global;

    if (ref->kind == NkIrRef_Param) {
        NK_ITERATE(NkIrParam const *, param, ctx->proc.params) {
            if (param->name == ref->sym && param->type->kind == NkIrType_Aggregate) {
                is_ptr = true;
                break;
            }
        }

        if (ctx->proc.ret.name == ref->sym) {
            is_ptr = true;
        }
    }

    return is_ptr;
}

static NkString intToPtr(Context *ctx, NkStream out, NkIrRef const *ref, NkIrType int_t) {
    NkStringBuilder sb = {.alloc = nk_arena_getAllocator(ctx->scratch)};
    NkStream tmp = nksb_getStream(&sb);

    if (refIsPtr(ctx, ref)) {
        emitRefUntyped(tmp, ref);
    } else {
        usize const reg = ctx->proc.next_local++;

        NkIrType const type = int_t ? int_t : ref->type;

        nk_printf(out, "%%.%zu = inttoptr ", reg);
        emitType(out, type);
        nk_printf(out, " ");
        emitRefUntyped(out, ref);
        nk_printf(out, " to ptr\n  ");

        nk_printf(tmp, "%%.%zu", reg);
    }

    return (NkString){NKS_INIT(sb)};
}

static NkString ptrToInt(Context *ctx, NkStream out, NkIrRef const *ref) {
    NkStringBuilder sb = {.alloc = nk_arena_getAllocator(ctx->scratch)};
    NkStream tmp = nksb_getStream(&sb);

    if (refIsPtr(ctx, ref)) {
        usize const reg = ctx->proc.next_local++;

        nk_printf(out, "%%.%zu = ptrtoint ptr ", reg);
        emitRefUntyped(out, ref);
        nk_printf(out, " to ");
        emitType(out, ref->type);
        nk_printf(out, "\n  ");

        nk_printf(tmp, "%%.%zu", reg);
    } else {
        emitRefUntyped(tmp, ref);
    }

    return (NkString){NKS_INIT(sb)};
}

static void emitLabel(Context *ctx, NkStream out, NkIrInstr const *instr, usize arg_idx) {
    NkIrArg const *arg = &instr->arg[arg_idx];
    usize const instr_idx = NK_INDEX(instr, ctx->proc.instrs);

    nk_assert(arg->kind == NkIrArg_Label || arg->kind == NkIrArg_LabelRel);

    Label const *label = NULL;

    switch (arg->kind) {
        case NkIrArg_Label:
            label = ctx->proc.instrs.data[instr_idx].code == NkIrOp_label
                        ? findLabelByIdx(ctx->proc.labels, instr_idx)
                        : findLabelByName(ctx->proc.labels, arg->label);
            break;

        case NkIrArg_LabelRel: {
            usize const target_idx = instr_idx + arg->offset;
            label = findLabelByIdx(ctx->proc.labels, target_idx);
            break;
        }

        default:
            nk_assert(!"unreachable");
            break;
    }

    nk_assert(label && "invalid label");

    u32 const label_idx = ctx->proc.indices[NK_INDEX(label, ctx->proc.labels)];
    if (label_idx) {
        nk_printf(out, "%s%u", nk_atom2cs(label->name), label_idx);
    } else {
        nk_printf(out, "%s", nk_atom2cs(label->name));
    }
}

typedef enum {
    Prefix_Sign = 1 << 0,
} PrefixMask;

static void emitBinop(Context *ctx, NkStream out, NkIrInstr const *instr, char const *name, PrefixMask mask) {
    NkIrRef const *ref0 = &instr->arg[0].ref;
    NkIrRef const *ref1 = &instr->arg[1].ref;
    NkIrRef const *ref2 = &instr->arg[2].ref;

    NkString const lhs = ptrToInt(ctx, out, ref1);
    NkString const rhs = ptrToInt(ctx, out, ref2);

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
    nk_printf(out, " " NKS_FMT ", " NKS_FMT, NKS_ARG(lhs), NKS_ARG(rhs));
}

static void emitLogic(Context *ctx, NkStream out, NkIrInstr const *instr, char const *name, PrefixMask mask) {
    NkIrRef const *ref0 = &instr->arg[0].ref;
    NkIrRef const *ref1 = &instr->arg[1].ref;
    NkIrRef const *ref2 = &instr->arg[2].ref;

    NkString const lhs = ptrToInt(ctx, out, ref1);
    NkString const rhs = ptrToInt(ctx, out, ref2);

    NkIrType const type = ref1->type;

    nk_assert(type->id == ref2->type->id);
    nk_assert(type->kind == NkIrType_Numeric);
    nk_assert(NKIR_NUMERIC_IS_INT(type->num));

    char const *opcode_prefix = "";

    if ((mask & Prefix_Sign) && NKIR_NUMERIC_IS_INT(type->num)) {
        if (NKIR_NUMERIC_IS_SIGNED(type->num)) {
            opcode_prefix = "a";
        } else {
            opcode_prefix = "l";
        }
    }

    emitRefUntyped(out, ref0);
    nk_printf(out, " = %s%s ", opcode_prefix, name);
    emitType(out, type);
    nk_printf(out, " " NKS_FMT ", " NKS_FMT, NKS_ARG(lhs), NKS_ARG(rhs));
}

static void emitCondJmp(Context *ctx, NkStream out, NkIrInstr const *instr, char const *cond) {
    NkIrRef const *ref1 = &instr->arg[1].ref;

    usize const label = ctx->proc.next_label++;
    usize const reg = ctx->proc.next_local++;

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

    usize const reg = ctx->proc.next_local++;

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
    nk_printf(out, ", ");
    emitRefUntyped(out, ref2);
    nk_printf(out, "\n  ");
    emitRefUntyped(out, ref0);
    nk_printf(out, " = %sext i1 %%.%zu to ", ext_prefix, reg);
    emitRefType(out, ref0);
}

static void emitMov(Context *ctx, NkStream out, NkIrInstr const *instr) {
    NkIrRef const *ref0 = &instr->arg[0].ref;
    NkIrRef const *ref1 = &instr->arg[1].ref;

    NkString const lhs = ptrToInt(ctx, out, ref0);
    NkString const rhs = ptrToInt(ctx, out, ref1);

    nk_printf(out, NKS_FMT " = bitcast ", NKS_ARG(lhs));
    emitType(out, ref1->type);
    nk_printf(out, " " NKS_FMT " to ", NKS_ARG(rhs));
    emitRefType(out, ref0);
}

static void emitCast(Context *ctx, NkStream out, NkIrInstr const *instr) {
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

    NkString const lhs = ptrToInt(ctx, out, ref0);
    NkString const rhs = ptrToInt(ctx, out, ref1);

    nk_printf(out, NKS_FMT " = %s%s%s ", NKS_ARG(lhs), src, op, dst);
    emitType(out, ref1->type);
    nk_printf(out, " " NKS_FMT " to ", NKS_ARG(rhs));
    emitRefType(out, ref0);
}

static void emitInstr(Context *ctx, NkStream out, NkIrInstr const *instr) {
    if (instr->code == NkIrOp_nop) {
        return;
    }

    if (instr->code != NkIrOp_label) {
        nk_printf(out, "  ");
    }

    NkIrRef const *ref0 = &instr->arg[0].ref;
    NkIrRef const *ref1 = &instr->arg[1].ref;

    switch ((NkIrOpcode)instr->code) {
        case NkIrOp_nop:
            break;

        case NkIrOp_mov:
            emitMov(ctx, out, instr);
            break;

        case NkIrOp_cast:
            emitCast(ctx, out, instr);
            break;

        case NkIrOp_comment:
            nk_printf(out, "; " NKS_FMT, NKS_ARG(instr->arg[1].str));
            break;

        case NkIrOp_alloc: {
            usize const reg = ctx->proc.next_local++;
            nk_printf(out, "%%.%zu = alloca ", reg);
            emitType(out, instr->arg[1].type);
            nk_printf(out, "\n  ");
            emitRefUntyped(out, ref0);
            nk_printf(out, " = ptrtoint ptr %%.%zu to ", reg);
            emitRefType(out, ref0);
            break;
        }

        case NkIrOp_load: {
            NkString const ptr = intToPtr(ctx, out, ref1, NULL);
            emitRefUntyped(out, ref0);
            nk_printf(out, " = load ");
            emitRefType(out, ref0);
            nk_printf(out, ", ptr ");
            nk_printf(out, NKS_FMT, NKS_ARG(ptr));
            break;
        }

        case NkIrOp_store: {
            NkString const ptr = intToPtr(ctx, out, ref0, NULL);
            nk_printf(out, "store ");
            emitRef(out, ref1);
            nk_printf(out, ", ptr ");
            nk_printf(out, NKS_FMT, NKS_ARG(ptr));
            break;
        }

        case NkIrOp_jmp:
            nk_printf(out, "br label %%");
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
            nk_printf(out, ":");
            break;

        case NkIrOp_add:
            emitBinop(ctx, out, instr, "add", 0);
            break;
        case NkIrOp_sub:
            emitBinop(ctx, out, instr, "sub", 0);
            break;
        case NkIrOp_mul:
            emitBinop(ctx, out, instr, "mul", 0);
            break;

        case NkIrOp_div:
            emitBinop(ctx, out, instr, "div", Prefix_Sign);
            break;
        case NkIrOp_mod:
            emitBinop(ctx, out, instr, "rem", Prefix_Sign);
            break;

        case NkIrOp_and:
            emitLogic(ctx, out, instr, "and", 0);
            break;
        case NkIrOp_or:
            emitLogic(ctx, out, instr, "or", 0);
            break;
        case NkIrOp_xor:
            emitLogic(ctx, out, instr, "xor", 0);
            break;
        case NkIrOp_lsh:
            emitLogic(ctx, out, instr, "shl", 0);
            break;
        case NkIrOp_rsh:
            emitLogic(ctx, out, instr, "shr", Prefix_Sign);
            break;

        case NkIrOp_call: {
            NkIrRefArray const arg_refs = instr->arg[2].refs;

            NkString const proc = intToPtr(ctx, out, ref1, NULL);
            NkString dst = {0};

            bool sret = false;
            if (ref0->kind && ref0->kind != NkIrRef_Null) {
                if (ref0->type->kind == NkIrType_Aggregate && ref0->type->size) {
                    NkIrType_T const ptr_t = {
                        // TODO: Hardcoded ptr type
                        .num = Int64,
                        .size = 8,
                        .align = 8,
                        .id = 0,
                        .kind = NkIrType_Numeric,
                    };
                    dst = intToPtr(ctx, out, ref0, &ptr_t);
                    sret = true;
                } else {
                    emitRefUntyped(out, ref0);
                    nk_printf(out, " = ");
                }
            }

            nk_printf(out, "call ");
            if (sret || !ref0->kind) {
                nk_printf(out, "void");
            } else {
                emitType(out, ref0->type);
            }

            nk_printf(out, " (");
            if (sret) {
                nk_printf(out, "ptr");
            }

            NK_ITERATE(NkIrRef const *, arg_ref, arg_refs) {
                if (NK_INDEX(arg_ref, arg_refs) || sret) {
                    nk_printf(out, ", ");
                }
                emitRefType(out, arg_ref);
                if (arg_ref->kind == NkIrRef_VariadicMarker) {
                    break;
                }
            }

            nk_printf(out, ") " NKS_FMT "(", NKS_ARG(proc));

            if (sret) {
                nk_printf(out, "ptr sret(");
                emitType(out, ref0->type);
                nk_printf(out, ") align %u " NKS_FMT, ref0->type->align, NKS_ARG(dst));
            }

            NK_ITERATE(NkIrRef const *, arg_ref, arg_refs) {
                if (arg_ref->kind == NkIrRef_VariadicMarker) {
                    continue;
                }
                if (NK_INDEX(arg_ref, arg_refs) || sret) {
                    nk_printf(out, ", ");
                }
                emitRef(out, arg_ref);
            }

            nk_printf(out, ")");

            break;
        }

        case NkIrOp_ret:
            nk_printf(out, "ret ");
            if (ref1->kind) {
                emitRef(out, ref1);
            } else {
                nk_printf(out, "void");
            }
            break;
    }

    nk_printf(out, "\n");
}

static void emitVal(NkStream out, void *base_addr, usize base_offset, NkIrRelocArray relocs, NkIrType type) {
    nk_assert(base_addr && "trying to inspect nullptr");

    emitTypeEx(out, type, base_offset, relocs);
    nk_printf(out, " ");

    switch (type->kind) {
        case NkIrType_Aggregate:
            nk_printf(out, "{");
            NK_ITERATE(NkIrAggregateElemInfo const *, elem, type->aggr) {
                if (NK_INDEX(elem, type->aggr)) {
                    nk_printf(out, ", ");
                }
                usize offset = base_offset + elem->offset;
                if (elem->count > 1) {
                    nk_printf(out, "[%u x ", elem->count);
                    emitTypeEx(out, elem->type, offset, relocs);
                    nk_printf(out, "] ");
                }
                if (elem->type->kind == NkIrType_Numeric && elem->type->size == 1) {
                    char const *addr = (char *)base_addr + offset;
                    nk_printf(out, "c\"");
                    nks_sanitize(out, (NkString){addr, type->aggr.data[0].count});
                    nk_printf(out, "\"");
                } else {
                    if (elem->count > 1) {
                        nk_printf(out, "[");
                    }
                    for (usize i = 0; i < elem->count; i++) {
                        if (i) {
                            nk_printf(out, ", ");
                        }
                        bool found_reloc = false;
                        NK_ITERATE(NkIrReloc const *, reloc, relocs) {
                            if (reloc->offset == offset && elem->type->size == 8) { // TODO: Hardcoded ptr size
                                nk_printf(out, "ptr ");
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
                        nk_printf(out, "]");
                    }
                }
            }
            nk_printf(out, "}");
            break;

        case NkIrType_Numeric: {
            void *addr = (u8 *)base_addr + base_offset;
            if (NKIR_NUMERIC_IS_INT(type->num)) {
                nkir_inspectVal(addr, type, out);
            } else {
                emitFloat(out, addr, type->num);
            }
            break;
        }
    }
}

static void emitData(NkStream out, NkIrData const *data) {
    nk_printf(out, "%s ", (data->flags & NkIrData_ReadOnly) ? "constant" : "global");

    if (data->addr) {
        emitVal(out, data->addr, 0, data->relocs, data->type);
    } else {
        emitType(out, data->type);
        switch (data->type->kind) {
            case NkIrType_Aggregate:
                nk_printf(out, " zeroinitializer");
                break;

            case NkIrType_Numeric:
                nk_printf(out, " 0");
                break;
        }
    }

    nk_printf(out, "\n");
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
                .proc =
                    {
                        .instrs = sym->proc.instrs,

                        .labels = labels,
                        .indices = indices,

                        .params = sym->proc.params,
                        .ret = sym->proc.ret,
                    },
            };

            nk_printf(out, "define ");
            emitVisibility(out, sym->vis);
            nk_printf(out, " ");
            if (ctx.proc.ret.name) {
                nk_printf(out, "void");
            } else {
                emitType(out, sym->proc.ret.type);
            }
            nk_printf(out, " ");
            emitGlobal(out, sym->name);
            nk_printf(out, "(");
            if (ctx.proc.ret.name) {
                nk_printf(out, "ptr sret(");
                emitType(out, ctx.proc.ret.type);
                nk_printf(out, ") align %u ", ctx.proc.ret.type->align);
                emitLocal(out, ctx.proc.ret.name);
            }
            NK_ITERATE(NkIrParam const *, param, ctx.proc.params) {
                if (NK_INDEX(param, sym->proc.params) || ctx.proc.ret.name) {
                    nk_printf(out, ", ");
                }
                if (param->type->kind == NkIrType_Aggregate) {
                    nk_printf(out, "ptr byval(");
                }
                emitType(out, param->type);
                if (param->type->kind == NkIrType_Aggregate) {
                    nk_printf(out, ") align %u", param->type->align);
                }
                nk_printf(out, " ");
                emitLocal(out, param->name);
            }
            nk_printf(out, ") {\n");
            NK_ITERATE(NkIrInstr const *, instr, sym->proc.instrs) {
                emitInstr(&ctx, out, instr);
            }
            nk_printf(out, "}\n");
            break;
        }

        case NkIrSymbol_Data:
            emitGlobal(out, sym->name);
            nk_printf(out, " = ");
            emitVisibility(out, sym->vis);
            nk_printf(out, " ");
            emitData(out, &sym->data);
            break;

        case NkIrSymbol_ExternProc: {
            bool const sret = sym->extern_proc.ret_type->size > 8; // TODO: Hardcoded ptr size
            nk_printf(out, "declare ");
            if (sret) {
                nk_printf(out, "void");
            } else {
                emitType(out, sym->extern_proc.ret_type);
            }
            nk_printf(out, " ");
            emitGlobal(out, sym->name);
            nk_printf(out, "(");
            if (sret) {
                nk_printf(out, "ptr sret(");
                emitType(out, sym->extern_proc.ret_type);
                nk_printf(out, ") align %u", sym->extern_proc.ret_type->align);
            }
            NK_ITERATE(NkIrType const *, type, sym->extern_proc.param_types) {
                if (NK_INDEX(type, sym->extern_proc.param_types) || sret) {
                    nk_printf(out, ", ");
                }
                if ((*type)->kind == NkIrType_Aggregate) {
                    nk_printf(out, "ptr byval(");
                }
                emitType(out, *type);
                if ((*type)->kind == NkIrType_Aggregate) {
                    nk_printf(out, ") align %u", (*type)->align);
                }
            }
            if (sym->extern_proc.flags & NkIrProc_Variadic) {
                nk_printf(out, ", ...");
            }
            nk_printf(out, ")\n");
            break;
        }

        case NkIrSymbol_ExternData:
            emitGlobal(out, sym->name);
            nk_printf(out, " = external global ");
            emitType(out, sym->extern_data.type);
            nk_printf(out, "\n");
            break;
    }

    nk_printf(out, "\n");
}

void nkir_emit_llvm(NkStream out, NkArena *scratch, NkIrSymbolArray mod) {
    NK_ARENA_SCOPE(scratch) {
        NK_ITERATE(NkIrSymbol const *, sym, mod) {
            emitSymbol(out, scratch, sym);
        }

        // TODO: Inserting name for libc compatibility main
        nk_printf(
            out,
            "define dso_local i32 @main() {\n\
  call void () @_entry()\n\
  ret i32 0\n\
}\n");
    }
}
