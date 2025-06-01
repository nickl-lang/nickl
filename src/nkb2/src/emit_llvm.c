#include "emit_llvm.h"

#include "nkb/ir.h"
#include "nkb/types.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"
// #include "ntk/log.h"

// NK_LOG_USE_SCOPE(emit_llvm);

static void writeType(NkStream out, NkIrType type) {
    if (!type) {
        return;
    }

    switch (type->kind) {
        case NkIrType_Aggregate:
            if (type->size) {
                // TODO: Only writing string types
                nk_printf(out, "[%u x ", type->aggr.data[0].count);
                writeType(out, type->aggr.data[0].type);
                nk_printf(out, "]");
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

static void writeVisibility(NkStream out, NkIrVisibility vis) {
    switch (vis) {
        case NkIrVisibility_Hidden:
            nk_assert(!"TODO not implemented");
            break;
        case NkIrVisibility_Default:
            break;
        case NkIrVisibility_Protected:
            nk_assert(!"TODO not implemented");
            break;
        case NkIrVisibility_Internal:
            nk_assert(!"TODO not implemented");
            break;
        case NkIrVisibility_Local:
            nk_printf(out, "private");
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

static void writeInstr(NkStream out, NkIrInstr const *instr) {
    if (instr->code != NkIrOp_label) {
        nk_printf(out, "  ");
    }

    switch (instr->code) {
        case NkIrOp_alloc:
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = alloca ");
            writeType(out, instr->arg[1].type);
            break;

        case NkIrOp_load:
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = load ");
            writeRefType(out, &instr->arg[0].ref);
            nk_printf(out, ", ptr ");
            writeRefUntyped(out, &instr->arg[1].ref);
            break;

        case NkIrOp_store:
            nk_printf(out, "store ");
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", ptr ");
            writeRefUntyped(out, &instr->arg[0].ref);
            break;

        case NkIrOp_jmp:
            nk_printf(out, "br label %%");
            writeName(out, instr->arg[1].label);
            break;

        case NkIrOp_jmpz:
            nk_printf(out, "br i1 ");
            writeRefUntyped(out, &instr->arg[1].ref);
            // TODO: Count inserted labels
            nk_printf(out, ", label %%.nonzero");
            nk_printf(out, ", label %%");
            writeName(out, instr->arg[2].label);
            nk_printf(out, "\n.nonzero:");
            break;

        case NkIrOp_jmpnz:
            nk_printf(out, "br i1 ");
            writeRefUntyped(out, &instr->arg[1].ref);
            nk_printf(out, ", label %%");
            writeName(out, instr->arg[2].label);
            // TODO: Count inserted labels
            nk_printf(out, ", label %%.zero\n.zero:");
            break;

        case NkIrOp_cmp_lt:
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_assert(instr->arg[1].ref.type->kind == NkIrType_Numeric);
            // TODO: Hardcoded integer cmp
            nk_printf(out, " = icmp %clt ", NKIR_NUMERIC_IS_SIGNED(instr->arg[1].ref.type->num) ? 's' : 'u');
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", ");
            writeRefUntyped(out, &instr->arg[2].ref);
            break;

        case NkIrOp_mov:
            nk_assert(instr->arg[1].ref.type->kind == NkIrType_Numeric);
            // TODO: Expressing mov as add
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = %sadd ", NKIR_NUMERIC_IS_FLT(instr->arg[1].ref.type->num) ? "f" : "");
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", 0");
            if (NKIR_NUMERIC_IS_FLT(instr->arg[1].ref.type->num)) {
                nk_printf(out, ".");
            }
            break;

        case NkIrOp_label:
            writeName(out, instr->arg[1].label);
            nk_printf(out, ":");
            break;

        case NkIrOp_add:
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = %sadd ", NKIR_NUMERIC_IS_FLT(instr->arg[1].ref.type->num) ? "f" : "");
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", ");
            writeRefUntyped(out, &instr->arg[2].ref);
            break;
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
            NkIrRef const *dst_ref = &instr->arg[0].ref;
            if (dst_ref->kind && dst_ref->kind != NkIrRef_Null) {
                writeRefUntyped(out, dst_ref);
                nk_printf(out, " = ");
            }
            nk_printf(out, "call ");
            if (dst_ref->kind) {
                writeType(out, dst_ref->type);
                nk_printf(out, " ");
            } else {
                nk_printf(out, "void ");
            }
            nk_printf(out, "(");
            NK_ITERATE(NkIrRef const *, arg_ref, instr->arg[2].refs) {
                if (NK_INDEX(arg_ref, instr->arg[2].refs)) {
                    nk_printf(out, ", ");
                }
                writeRefType(out, arg_ref);
                if (arg_ref->kind == NkIrRef_VariadicMarker) {
                    break;
                }
            }
            nk_printf(out, ") ");
            writeRefUntyped(out, &instr->arg[1].ref);
            nk_printf(out, "(");
            NK_ITERATE(NkIrRef const *, arg_ref, instr->arg[2].refs) {
                if (arg_ref->kind == NkIrRef_VariadicMarker) {
                    continue;
                }
                if (NK_INDEX(arg_ref, instr->arg[2].refs)) {
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

void writeData(NkStream out, NkIrData const *data) {
    nk_printf(out, "%s ", (data->flags & NkIrData_ReadOnly) ? "constant" : "global");

    writeType(out, data->type);

    // TODO: Ignoring relocs

    switch (data->type->kind) {
        case NkIrType_Aggregate:
            // TODO: Ignoring aggregate types other than string
            nk_printf(out, " c\"");
            nks_sanitize(out, (NkString){data->addr, data->type->aggr.data[0].count});
            nk_printf(out, "\"");
            break;

        case NkIrType_Numeric:
            nk_printf(out, " ");
            nkir_inspectVal(data->addr, data->type, out);
            break;
    }

    nk_printf(out, "\n");
}

void nkir_emit_llvm(NkStream out, NkIrModule mod) {
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

            case NkIrSymbol_Proc:
                nk_printf(out, "define ");
                writeType(out, sym->proc.ret.type);
                nk_printf(out, " ");
                writeGlobal(out, sym->name);
                nk_printf(out, "(");
                NK_ITERATE(NkIrParam const *, param, sym->proc.params) {
                    if (NK_INDEX(param, sym->proc.params)) {
                        nk_printf(out, ", ");
                    }
                    writeType(out, param->type);
                    nk_printf(out, " ");
                    writeLocal(out, param->name);
                }
                nk_printf(out, ") {\n");
                NK_ITERATE(NkIrInstr const *, instr, sym->proc.instrs) {
                    writeInstr(out, instr);
                }
                nk_printf(out, "}\n");
                break;
        }
        nk_printf(out, "\n");
    }

    // TODO: Inserting name for libc compatibility main
    nk_printf(
        out,
        "define i32 @main() {\n\
  call void () @_entry()\n\
  ret i32 0\n\
}\n");
}
