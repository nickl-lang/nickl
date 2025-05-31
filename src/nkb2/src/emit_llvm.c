#include "emit_llvm.h"

#include "nkb/ir.h"
#include "nkb/types.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/utils.h"

static void writeType(NkStream out, NkIrType type) {
    if (!type) {
        return;
    }

    switch (type->kind) {
        case NkIrType_Aggregate:
            // TODO: Treating aggregate as void
            nk_printf(out, "void");
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
                    nk_printf(out, "f32");
                    break;
                case Float64:
                    nk_printf(out, "f64");
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

        case NkIrRef_Imm:
            nkir_inspectVal((void *)&ref->imm, ref->type, out);
            break;

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
            // TODO: Expressing mov as add
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = add ");
            writeRef(out, &instr->arg[1].ref);
            nk_printf(out, ", 0");
            break;

        case NkIrOp_label:
            writeName(out, instr->arg[1].label);
            nk_printf(out, ":");
            break;

        case NkIrOp_add:
            writeRefUntyped(out, &instr->arg[0].ref);
            nk_printf(out, " = add ");
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

void nkir_emit_llvm(NkStream out, NkIrModule mod) {
    NK_ITERATE(NkIrSymbol const *, sym, mod) {
        switch (sym->kind) {
            case NkIrSymbol_Extern:
                // TODO: Add types to externs
                nk_printf(out, "@%s = external global ptr\n", nk_atom2cs(sym->name));
                break;

            case NkIrSymbol_Data:
                if (!sym->data.addr) {
                    nk_assert(!"TODO not implemented");
                }
                if (!(sym->data.flags & NkIrData_ReadOnly)) {
                    nk_assert(!"TODO not implemented");
                }
                if (sym->data.relocs.size) {
                    nk_assert(!"TODO not implemented");
                }
                if (sym->data.type->kind != NkIrType_Aggregate) {
                    nk_assert(!"TODO not implemented");
                }
                if (sym->data.type->aggr.size != 1) {
                    nk_assert(!"TODO not implemented");
                }
                if (sym->data.type->aggr.data[0].type->kind != NkIrType_Numeric) {
                    nk_assert(!"TODO not implemented");
                }
                if (sym->data.type->aggr.data[0].type->size != 1) {
                    nk_assert(!"TODO not implemented");
                }
                if (sym->vis != NkIrVisibility_Local) {
                    nk_assert(!"TODO not implemented");
                }

                writeGlobal(out, sym->name);
                nk_printf(out, " = ");
                writeVisibility(out, sym->vis);
                // TODO: Abstract constness printing
                nk_printf(out, " constant [%u x ", sym->data.type->aggr.data[0].count);
                writeType(out, sym->data.type->aggr.data[0].type);
                nk_printf(out, "] c\"");
                nks_sanitize(out, (NkString){sym->data.addr, sym->data.type->aggr.data[0].count});
                nk_printf(out, "\"\n");

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
    }

    // TODO: Inserting name for libc compatibility main
    nk_printf(
        out,
        "define i32 @main() {\n\
  call void () @_entry()\n\
  ret i32 0\n\
}\n");
}
