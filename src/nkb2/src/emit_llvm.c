#include "emit_llvm.h"

#include <unistd.h>

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

static void writeTypeEx(NkStream out, NkIrType type, usize base_offset, NkIrRelocArray relocs) {
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
                        writeTypeEx(out, elem->type, offset, relocs);
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

static void writeType(NkStream out, NkIrType type) {
    writeTypeEx(out, type, 0, (NkIrRelocArray){0});
}

static void writeVisibility(NkStream out, NkIrVisibility vis) {
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

static void writeName(NkStream out, NkAtom name) {
    NkString const name_str = nk_atom2s(name);
    if (name_str.size) {
        nk_printf(out, NKS_FMT, NKS_ARG(name_str));
    } else {
        nk_printf(out, "_%u", name);
    }
}

static void writeGlobal(NkStream out, NkAtom name) {
    nk_printf(out, "@");
    writeName(out, name);
}

static void writeLocal(NkStream out, NkAtom name) {
    nk_printf(out, "%%");
    writeName(out, name);
}

static void writeRefUntyped(NkStream out, NkIrRef const *ref) {
    switch (ref->kind) {
        case NkIrRef_None:
            break;

        case NkIrRef_Null:
            break;

        case NkIrRef_Local:
        case NkIrRef_Param:
            writeLocal(out, ref->sym);
            break;

        case NkIrRef_Global:
            writeGlobal(out, ref->sym);
            break;

        case NkIrRef_Imm: {
            // TODO: Print floats in hex
            // TODO: Investigate precision loss in ir
            NKSB_FIXED_BUFFER(sb, 128);
            NkStream s = NKIR_NUMERIC_IS_FLT(ref->type->num) ? nksb_getStream(&sb) : out;
            nkir_inspectVal((void *)&ref->imm, ref->type, s);
            if (NKIR_NUMERIC_IS_FLT(ref->type->num)) {
                usize i = 0;
                for (; i < sb.size; i++) {
                    if (sb.data[i] == '.') {
                        break;
                    }
                }
                nk_printf(out, NKS_FMT, NKS_ARG(sb));
                if (i == sb.size) {
                    nk_printf(out, ".");
                }
            }
            break;
        }

        case NkIrRef_VariadicMarker:
            nk_printf(out, "...");
            break;
    }
}

static void writeRefType(NkStream out, NkIrRef const *ref) {
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
            writeType(out, ref->type);
            break;

        case NkIrRef_VariadicMarker:
            nk_printf(out, "...");
            break;
    }
}

static void writeRef(NkStream out, NkIrRef const *ref) {
    writeRefType(out, ref);
    nk_printf(out, " ");
    writeRefUntyped(out, ref);
}

typedef struct {
    NkArena *scratch;

    NkIrInstrArray instrs;

    LabelArray labels;
    u32 *indices;

    NkIrParamArray proc_params;
    NkIrParam proc_ret;

    usize next_local;
    usize next_label;
} Context;

static bool refIsPtr(Context *ctx, NkIrRef const *ref) {
    bool is_ptr = ref->kind == NkIrRef_Global;

    if (ref->kind == NkIrRef_Param) {
        NK_ITERATE(NkIrParam const *, param, ctx->proc_params) {
            if (param->name == ref->sym && param->type->kind == NkIrType_Aggregate) {
                is_ptr = true;
                break;
            }
        }

        if (ctx->proc_ret.name && ctx->proc_ret.name == ref->sym) {
            is_ptr = true;
        }
    }

    return is_ptr;
}

static NkString refIntoPtr(Context *ctx, NkStream out, NkIrRef const *ref, NkIrType int_t) {
    NkStringBuilder sb = {.alloc = nk_arena_getAllocator(ctx->scratch)};
    NkStream tmp = nksb_getStream(&sb);

    if (refIsPtr(ctx, ref)) {
        writeRefUntyped(tmp, ref);
    } else {
        usize const reg = ctx->next_local++;

        NkIrType const type = int_t ? int_t : ref->type;

        nk_printf(out, "%%.%zu = inttoptr ", reg);
        writeType(out, type);
        nk_printf(out, " ");
        writeRefUntyped(out, ref);
        nk_printf(out, " to ptr\n  ");

        nk_printf(tmp, "%%.%zu", reg);
    }

    return (NkString){NKS_INIT(sb)};
}

static NkString refIntoInt(Context *ctx, NkStream out, NkIrRef const *ref) {
    NkStringBuilder sb = {.alloc = nk_arena_getAllocator(ctx->scratch)};
    NkStream tmp = nksb_getStream(&sb);

    if (refIsPtr(ctx, ref)) {
        usize const reg = ctx->next_local++;

        nk_printf(out, "%%.%zu = ptrtoint ptr ", reg);
        writeRefUntyped(out, ref);
        nk_printf(out, " to ");
        writeType(out, ref->type);
        nk_printf(out, "\n  ");

        nk_printf(tmp, "%%.%zu", reg);
    } else {
        writeRefUntyped(tmp, ref);
    }

    return (NkString){NKS_INIT(sb)};
}

static void writeLabel(Context *ctx, NkStream out, NkIrInstr const *instr, usize arg_idx) {
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

static void writeInstr(Context *ctx, NkStream out, NkIrInstr const *instr) {
    if (instr->code == NkIrOp_comment) {
        return;
    }

    if (instr->code != NkIrOp_label) {
        nk_printf(out, "  ");
    }

    switch (instr->code) {
        case NkIrOp_trunc:
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = trunc ");
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, " to ");
            writeRefType(out, &instr->arg[0].ref);
            break;

        case NkIrOp_alloc: {
            usize const reg = ctx->next_local++;
            nk_printf(out, "%%.%zu = alloca ", reg);
            writeType(out, instr->arg[1].type);
            nk_printf(out, "\n  ");
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = ptrtoint ptr %%.%zu to ", reg);
            writeRefType(out, &instr->arg[0].ref);
            break;
        }

        case NkIrOp_load: {
            NkString const ptr = refIntoPtr(ctx, out, &instr->arg[1].ref, NULL);
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = load ");
            writeRefType(out, &instr->arg[0].ref);
            nk_printf(out, ", ptr ");
            nk_printf(out, NKS_FMT, NKS_ARG(ptr));
            break;
        }

        case NkIrOp_store: {
            NkString const ptr = refIntoPtr(ctx, out, &instr->arg[0].ref, NULL);
            nk_printf(out, "store ");
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", ptr ");
            nk_printf(out, NKS_FMT, NKS_ARG(ptr));
            break;
        }

        case NkIrOp_jmp:
            nk_printf(out, "br label %%");
            writeLabel(ctx, out, instr, 1);
            break;

        case NkIrOp_jmpz: {
            usize const label = ctx->next_label++;
            usize const reg = ctx->next_local++;
            nk_printf(out, "%%.%zu = icmp eq ", reg);
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", 0\n  br i1 %%.%zu", reg);
            nk_printf(out, ", label %%");
            writeLabel(ctx, out, instr, 2);
            nk_printf(out, ", label %%.label%zu", label);
            nk_printf(out, "\n.label%zu:", label);
            break;
        }

        case NkIrOp_jmpnz: {
            usize const label = ctx->next_label++;
            usize const reg = ctx->next_local++;
            nk_printf(out, "%%.%zu = icmp ne ", reg);
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", 0\n  br i1 %%.%zu", reg);
            nk_printf(out, ", label %%");
            writeLabel(ctx, out, instr, 2);
            nk_printf(out, ", label %%.label%zu\n.label%zu:", label, label);
            break;
        }

        case NkIrOp_cmp_lt: {
            usize const reg = ctx->next_local++;
            nk_assert(instr->arg[1].ref.type->kind == NkIrType_Numeric);
            // TODO: Hardcoded integer cmp
            nk_printf(out, "%%.%zu = icmp %clt ", reg, NKIR_NUMERIC_IS_SIGNED(instr->arg[1].ref.type->num) ? 's' : 'u');
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", ");
            writeRefUntyped(out, &instr->arg[2].ref);
            nk_printf(out, "\n  ");
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(
                out, " = %cext i1 %%.%zu to ", NKIR_NUMERIC_IS_SIGNED(instr->arg[0].ref.type->num) ? 's' : 'z', reg);
            writeRefType(out, &instr->arg[0].ref);
            break;
        }

        case NkIrOp_mov:
            nk_assert(instr->arg[1].ref.type->kind == NkIrType_Numeric);
            NkString const arg = refIntoInt(ctx, out, &instr->arg[1].ref);
            // TODO: Expressing mov as add
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = %sadd ", NKIR_NUMERIC_IS_FLT(instr->arg[1].ref.type->num) ? "f" : "");
            writeType(out, instr->arg[1].ref.type);
            nk_printf(out, " ");
            nk_printf(out, NKS_FMT, NKS_ARG(arg));
            nk_printf(out, ", 0");
            if (NKIR_NUMERIC_IS_FLT(instr->arg[1].ref.type->num)) {
                nk_printf(out, ".");
            }
            break;

        case NkIrOp_label:
            writeLabel(ctx, out, instr, 1);
            nk_printf(out, ":");
            break;

        case NkIrOp_add: {
            NkString const lhs = refIntoInt(ctx, out, &instr->arg[1].ref);
            NkString const rhs = refIntoInt(ctx, out, &instr->arg[2].ref);
            NkIrType const type = instr->arg[1].ref.type;
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = %sadd ", NKIR_NUMERIC_IS_FLT(type->num) ? "f" : "");
            writeType(out, type);
            nk_printf(out, " ");
            nk_printf(out, NKS_FMT, NKS_ARG(lhs));
            nk_printf(out, ", ");
            nk_printf(out, NKS_FMT, NKS_ARG(rhs));
            break;
        }
        case NkIrOp_sub:
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = %ssub ", NKIR_NUMERIC_IS_FLT(instr->arg[1].ref.type->num) ? "f" : "");
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", ");
            writeRefUntyped(out, &instr->arg[2].ref);
            break;
        case NkIrOp_mul:
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = %smul ", NKIR_NUMERIC_IS_FLT(instr->arg[1].ref.type->num) ? "f" : "");
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", ");
            writeRefUntyped(out, &instr->arg[2].ref);
            break;
        case NkIrOp_div:
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = %sdiv ", NKIR_NUMERIC_IS_FLT(instr->arg[1].ref.type->num) ? "f" : "");
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", ");
            writeRefUntyped(out, &instr->arg[2].ref);
            break;

        case NkIrOp_call: {
            NkString const ptr = refIntoPtr(ctx, out, &instr->arg[1].ref, NULL);
            NkIrRef const *dst_ref = &instr->arg[0].ref;
            NkString dst_str = {0};
            bool sret = false;
            if (dst_ref->kind && dst_ref->kind != NkIrRef_Null) {
                if (dst_ref->type->kind == NkIrType_Aggregate && dst_ref->type->size) {
                    NkIrType_T const ptr_t = {
                        // TODO: Hardcoded ptr type
                        .num = Int64,
                        .size = 8,
                        .align = 8,
                        .id = 0,
                        .kind = NkIrType_Numeric,
                    };
                    dst_str = refIntoPtr(ctx, out, dst_ref, &ptr_t);
                    sret = true;
                } else {
                    writeRefUntyped(out, dst_ref);
                    nk_printf(out, " = ");
                }
            }
            nk_printf(out, "call ");
            if (sret || !dst_ref->kind) {
                nk_printf(out, "void ");
            } else {
                writeType(out, dst_ref->type);
                nk_printf(out, " ");
            }
            nk_printf(out, "(");
            if (sret) {
                nk_printf(out, "ptr");
            }
            NK_ITERATE(NkIrRef const *, arg_ref, instr->arg[2].refs) {
                if (NK_INDEX(arg_ref, instr->arg[2].refs) || sret) {
                    nk_printf(out, ", ");
                }
                writeRefType(out, arg_ref);
                if (arg_ref->kind == NkIrRef_VariadicMarker) {
                    break;
                }
            }
            nk_printf(out, ") ");
            nk_printf(out, NKS_FMT, NKS_ARG(ptr));
            nk_printf(out, "(");
            if (sret) {
                nk_printf(out, "ptr sret(");
                writeType(out, dst_ref->type);
                nk_printf(out, ") align %u " NKS_FMT, dst_ref->type->align, NKS_ARG(dst_str));
            }
            NK_ITERATE(NkIrRef const *, arg_ref, instr->arg[2].refs) {
                if (arg_ref->kind == NkIrRef_VariadicMarker) {
                    continue;
                }
                if (NK_INDEX(arg_ref, instr->arg[2].refs) || sret) {
                    nk_printf(out, ", ");
                }
                writeRef(out, arg_ref);
            }
            nk_printf(out, ")");
            break;
        }

        case NkIrOp_ret:
            nk_printf(out, "ret ");
            if (instr->arg[1].ref.kind) {
                writeRef(out, &instr->arg[1].ref);
            } else {
                nk_printf(out, "void");
            }
            break;

        default:
            nk_assert(!"TODO not implemented");
            break;
    }

    nk_printf(out, "\n");
}

static void inspectVal(NkStream out, void *base_addr, usize base_offset, NkIrRelocArray relocs, NkIrType type) {
    nk_assert(base_addr && "trying to inspect nullptr");

    writeTypeEx(out, type, base_offset, relocs);
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
                    writeTypeEx(out, elem->type, offset, relocs);
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
                                writeGlobal(out, reloc->sym);
                                found_reloc = true;
                                break;
                            }
                        }
                        if (!found_reloc) {
                            inspectVal(out, base_addr, offset, relocs, elem->type);
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
            nkir_inspectVal(addr, type, out);
            break;
        }
    }
}

static void writeData(NkStream out, NkIrData const *data) {
    nk_printf(out, "%s ", (data->flags & NkIrData_ReadOnly) ? "constant" : "global");

    // TODO: Ignoring relocs

    if (data->addr) {
        inspectVal(out, data->addr, 0, data->relocs, data->type);
    } else {
        writeType(out, data->type);
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

void nkir_emit_llvm(NkStream out, NkArena *scratch, NkIrModule mod) {
    NK_ITERATE(NkIrSymbol const *, sym, mod) {
        switch (sym->kind) {
            case NkIrSymbol_Extern:
                // TODO: Add types to externs
                nk_printf(out, "@%s = external global ptr\n", nk_atom2cs(sym->name));
                break;

            case NkIrSymbol_Data:
                writeGlobal(out, sym->name);
                nk_printf(out, " = ");
                writeVisibility(out, sym->vis);
                nk_printf(out, " ");
                writeData(out, &sym->data);
                break;

            case NkIrSymbol_Proc: {
                LabelDynArray da_labels = {.alloc = nk_arena_getAllocator(scratch)};
                LabelArray const labels = collectLabels(sym->proc.instrs, &da_labels);

                u32 *indices = countLabels(labels);

                Context ctx = {
                    .scratch = scratch,

                    .instrs = sym->proc.instrs,

                    .labels = labels,
                    .indices = indices,

                    .proc_params = sym->proc.params,
                    .proc_ret = sym->proc.ret,
                };

                nk_printf(out, "define ");
                writeVisibility(out, sym->vis);
                nk_printf(out, " ");
                if (ctx.proc_ret.name) {
                    nk_printf(out, "void");
                } else {
                    writeType(out, sym->proc.ret.type);
                }
                nk_printf(out, " ");
                writeGlobal(out, sym->name);
                nk_printf(out, "(");
                if (ctx.proc_ret.name) {
                    nk_printf(out, "ptr sret(");
                    writeType(out, ctx.proc_ret.type);
                    nk_printf(out, ") align %u ", ctx.proc_ret.type->align);
                    writeLocal(out, ctx.proc_ret.name);
                }
                NK_ITERATE(NkIrParam const *, param, ctx.proc_params) {
                    if (NK_INDEX(param, sym->proc.params) || ctx.proc_ret.name) {
                        nk_printf(out, ", ");
                    }
                    if (param->type->kind == NkIrType_Aggregate) {
                        nk_printf(out, "ptr byval(");
                    }
                    writeType(out, param->type);
                    if (param->type->kind == NkIrType_Aggregate) {
                        nk_printf(out, ") align %u", param->type->align);
                    }
                    nk_printf(out, " ");
                    writeLocal(out, param->name);
                }
                nk_printf(out, ") {\n");
                NK_ITERATE(NkIrInstr const *, instr, sym->proc.instrs) {
                    writeInstr(&ctx, out, instr);
                }
                nk_printf(out, "}\n");

                nk_freeTn(nk_default_allocator, indices, u32, labels.size); // TODO: Use scratch arena
                break;
            }
        }
        nk_printf(out, "\n");
    }

    // TODO: Inserting name for libc compatibility main
    nk_printf(
        out,
        "define dso_local i32 @main() {\n\
  call void () @_entry()\n\
  ret i32 0\n\
}\n");
}
