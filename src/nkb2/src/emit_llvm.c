#include "emit_llvm.h"

#include "nkb/ir.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/utils.h"

static void printType(NkStream out, NkIrType type) {
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
#define X(TYPE, VALUE_TYPE)    \
    case VALUE_TYPE:           \
        nk_printf(out, #TYPE); \
        break;
                NKIR_NUMERIC_ITERATE(X)
#undef X
            }
            break;
    }
}

static void printVisibility(NkStream out, NkIrVisibility vis) {
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

static void printName(NkStream out, NkAtom name) {
    NkString const name_str = nk_atom2s(name);
    if (name_str.size) {
        nk_printf(out, "@" NKS_FMT, NKS_ARG(name_str));
    } else {
        nk_printf(out, "@_%u", name);
    }
}

static void printRefUntyped(NkStream out, NkIrRef const *ref) {
    switch (ref->kind) {
        case NkIrRef_None:
            break;

        case NkIrRef_Null:
            break;

        case NkIrRef_Local:
            nk_assert(!"TODO not implemented");
            break;

        case NkIrRef_Param:
            nk_assert(!"TODO not implemented");
            break;

        case NkIrRef_Global:
            printName(out, ref->sym);
            break;

        case NkIrRef_Imm:
            nk_assert(!"TODO not implemented");
            break;

        case NkIrRef_VariadicMarker:
            nk_printf(out, "...");
            break;
    }
}

static void printRefType(NkStream out, NkIrRef const *ref) {
    switch (ref->kind) {
        case NkIrRef_None:

        case NkIrRef_Global:
            nk_printf(out, "ptr");
            break;

        case NkIrRef_Imm:
        case NkIrRef_Null:
        case NkIrRef_Local:
        case NkIrRef_Param:
            printType(out, ref->type);
            break;

        case NkIrRef_VariadicMarker:
            nk_printf(out, "...");
            break;
    }
}

static void printRef(NkStream out, NkIrRef const *ref) {
    printRefType(out, ref);
    nk_printf(out, " ");
    printRefUntyped(out, ref);
}

static void printInstr(NkStream out, NkIrInstr const *instr) {
    nk_printf(out, "  ");

    switch (instr->code) {
        case NkIrOp_call: {
            NkIrRef const *dst_ref = &instr->arg[0].ref;
            if (dst_ref->kind && dst_ref->kind != NkIrRef_Null) {
                printRef(out, dst_ref);
                nk_printf(out, " = ");
            }
            nk_printf(out, "call ");
            if (dst_ref->kind) {
                printType(out, dst_ref->type);
                nk_printf(out, " ");
            } else {
                nk_printf(out, "void ");
            }
            nk_printf(out, "(");
            NK_ITERATE(NkIrRef const *, arg_ref, instr->arg[2].refs) {
                if (NK_INDEX(arg_ref, instr->arg[2].refs)) {
                    nk_printf(out, ", ");
                }
                printRefType(out, arg_ref);
            }
            nk_printf(out, ") ");
            printRefUntyped(out, &instr->arg[1].ref);
            nk_printf(out, "(");
            NK_ITERATE(NkIrRef const *, arg_ref, instr->arg[2].refs) {
                if (arg_ref->kind == NkIrRef_VariadicMarker) {
                    continue;
                }
                if (NK_INDEX(arg_ref, instr->arg[2].refs)) {
                    nk_printf(out, ", ");
                }
                printRef(out, arg_ref);
            }
            nk_printf(out, ")");
            break;
        }

        case NkIrOp_ret:
            nk_printf(out, "ret ");
            if (instr->arg[1].ref.kind) {
                printRef(out, &instr->arg[1].ref);
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

                printName(out, sym->name);
                nk_printf(out, " = ");
                printVisibility(out, sym->vis);
                // TODO: Abstract constness printing
                nk_printf(out, " constant [%u x ", sym->data.type->aggr.data[0].count);
                printType(out, sym->data.type->aggr.data[0].type);
                nk_printf(out, "] c\"");
                nks_sanitize(out, (NkString){sym->data.addr, sym->data.type->aggr.data[0].count});
                nk_printf(out, "\"\n");

                break;

            case NkIrSymbol_Proc:
                nk_printf(out, "define ");
                printType(out, sym->proc.ret.type);
                nk_printf(out, " ");
                printName(out, sym->name);
                nk_printf(out, "() {\n");
                NK_ITERATE(NkIrInstr const *, instr, sym->proc.instrs) {
                    printInstr(out, instr);
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
