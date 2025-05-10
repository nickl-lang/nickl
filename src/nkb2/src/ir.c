#include "nkb/ir.h"

#include "nkb/types.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/log.h"
#include "ntk/stream.h"

NK_LOG_USE_SCOPE(ir);

static NkIrArg argNull() {
    return (NkIrArg){0};
}

static NkIrArg argRef(NkIrRef ref) {
    return (NkIrArg){
        .ref = ref,
        .kind = NkIrArg_Ref,
    };
}

static NkIrArg argRefArray(NkIrRefArray refs) {
    return (NkIrArg){
        .refs = refs,
        .kind = NkIrArg_RefArray,
    };
}

static NkIrArg argLabel(NkAtom label) {
    return (NkIrArg){
        .label = {label, 0},
        .kind = NkIrArg_Label,
    };
}

static NkIrArg argType(NkIrType type) {
    return (NkIrArg){
        .type = type,
        .kind = NkIrArg_Type,
    };
}

static NkIrArg argString(NkString str) {
    return (NkIrArg){
        .str = str,
        .kind = NkIrArg_String,
    };
}

static NkIrArg argIdx(u32 idx) {
    return (NkIrArg){
        .idx = idx,
        .kind = NkIrArg_Idx,
    };
}

void nkir_convertToPic(NkIrInstrArray instrs, NkIrInstrDynArray *out) {
    NK_LOG_TRC("%s", __func__);

    (void)instrs;
    (void)out;
    nk_assert(!"TODO: `nkir_convertToPic` not implemented");
}

NkIrRef nkir_makeRefNull(NkIrType type) {
    return (NkIrRef){
        .type = type,
        .kind = NkIrRef_Null,
    };
}

NkIrRef nkir_makeRefLocal(NkAtom sym, NkIrType type) {
    return (NkIrRef){
        .sym = sym,
        .type = type,
        .kind = NkIrRef_Local,
    };
}

NkIrRef nkir_makeRefParam(NkAtom sym, NkIrType type) {
    return (NkIrRef){
        .sym = sym,
        .type = type,
        .kind = NkIrRef_Param,
    };
}

NkIrRef nkir_makeRefGlobal(NkAtom sym, NkIrType type) {
    return (NkIrRef){
        .sym = sym,
        .type = type,
        .kind = NkIrRef_Global,
    };
}

NkIrRef nkir_makeRefImm(NkIrImm imm, NkIrType type) {
    return (NkIrRef){
        .imm = imm,
        .type = type,
        .kind = NkIrRef_Imm,
    };
}

NkIrRef nkir_makeVariadicMarker() {
    return (NkIrRef){
        .kind = NkIrRef_VariadicMarker,
    };
}

NkIrInstr nkir_make_nop() {
    return (NkIrInstr){
        .arg = {argNull(), argNull(), argNull()},
        .code = NkIrOp_nop,
    };
}

NkIrInstr nkir_make_ret(NkIrRef arg) {
    return (NkIrInstr){
        .arg = {argNull(), argRef(arg), argNull()},
        .code = NkIrOp_ret,
    };
}

NkIrInstr nkir_make_jmp(NkAtom label) {
    return (NkIrInstr){
        .arg = {argNull(), argLabel(label), argNull()},
        .code = NkIrOp_jmp,
    };
}

NkIrInstr nkir_make_jmpz(NkIrRef cond, NkAtom label) {
    return (NkIrInstr){
        .arg = {argNull(), argRef(cond), argLabel(label)},
        .code = NkIrOp_jmpz,
    };
}

NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkAtom label) {
    return (NkIrInstr){
        .arg = {argNull(), argRef(cond), argLabel(label)},
        .code = NkIrOp_jmpnz,
    };
}

NkIrInstr nkir_make_call(NkIrRef dst, NkIrRef proc, NkIrRefArray args) {
    return (NkIrInstr){
        .arg = {argRef(dst), argRef(proc), argRefArray(args)},
        .code = NkIrOp_call,
    };
}

NkIrInstr nkir_make_store(NkIrRef ptr, NkIrRef src) {
    return (NkIrInstr){
        .arg = {argNull(), argRef(ptr), argRef(src)},
        .code = NkIrOp_store,
    };
}

NkIrInstr nkir_make_load(NkIrRef dst, NkIrRef ptr) {
    return (NkIrInstr){
        .arg = {argRef(dst), argRef(ptr), argNull()},
        .code = NkIrOp_load,
    };
}

NkIrInstr nkir_make_alloc(NkIrRef dst, NkIrType type) {
    return (NkIrInstr){
        .arg = {argRef(dst), argType(type), argNull()},
        .code = NkIrOp_alloc,
    };
}

#define UNA_IR(NAME)                                               \
    NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef arg) { \
        return (NkIrInstr){                                        \
            .arg = {argRef(dst), argRef(arg), argNull()},          \
            .code = NK_CAT(NkIrOp_, NAME),                         \
        };                                                         \
    }
#define BIN_IR(NAME)                                                            \
    NkIrInstr NK_CAT(nkir_make_, NAME)(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) { \
        return (NkIrInstr){                                                     \
            .arg = {argRef(dst), argRef(lhs), argRef(rhs)},                     \
            .code = NK_CAT(NkIrOp_, NAME),                                      \
        };                                                                      \
    }
#define DBL_IR(NAME1, NAME2)                                                                               \
    NkIrInstr NK_CAT(nkir_make_, NK_CAT(NAME1, NK_CAT(_, NAME2)))(NkIrRef dst, NkIrRef lhs, NkIrRef rhs) { \
        return (NkIrInstr){                                                                                \
            .arg = {argRef(dst), argRef(lhs), argRef(rhs)},                                                \
            .code = NK_CAT(NkIrOp_, NK_CAT(NAME1, NK_CAT(_, NAME2))),                                      \
        };                                                                                                 \
    }
#include "nkb/ir.inl"

NkIrInstr nkir_make_label(NkAtom label) {
    return (NkIrInstr){
        .arg = {argNull(), argLabel(label), argNull()},
        .code = NkIrOp_label,
    };
}

NkIrInstr nkir_make_file(NkString file) {
    return (NkIrInstr){
        .arg = {argNull(), argString(file), argNull()},
        .code = NkIrOp_file,
    };
}

NkIrInstr nkir_make_line(u32 line) {
    return (NkIrInstr){
        .arg = {argNull(), argIdx(line), argNull()},
        .code = NkIrOp_line,
    };
}

NkIrInstr nkir_make_comment(NkString comment) {
    return (NkIrInstr){
        .arg = {argNull(), argString(comment), argNull()},
        .code = NkIrOp_comment,
    };
}

void nkir_exportModule(NkIrModule m, NkString path /*, c_compiler_config */) {
    NK_LOG_TRC("%s", __func__);

    (void)m;
    (void)path;
    nk_assert(!"TODO: `nkir_exportModule` not implemented");
}

NkIrRunCtx nkir_createRunCtx(NkIrModule *m, NkArena *arena) {
    NK_LOG_TRC("%s", __func__);

    (void)m;
    (void)arena;
    nk_assert(!"TODO: `nkir_createRunCtx` not implemented");
}

bool nkir_invoke(NkIrRunCtx ctx, NkAtom sym, void **args, void **ret) {
    NK_LOG_TRC("%s", __func__);

    (void)ctx;
    (void)sym;
    (void)args;
    (void)ret;
    nk_assert(!"TODO: `nkir_invoke` not implemented");
}

void nkir_inspectModule(NkIrModule m, NkStream out) {
    NK_LOG_TRC("%s", __func__);

    for (usize i = 0; i < m.size; i++) {
        nk_stream_printf(out, "\n");
        NkIrSymbol const *sym = &m.data[i];
        nkir_inspectSymbol(sym, out);
    }
}

static char const *s_opcode_names[] = {
#define IR(NAME) #NAME,
#define DBL_IR(NAME1, NAME2) #NAME1 " " #NAME2,
#include "nkb/ir.inl"
};

static bool isAddressableInstr(u8 code) {
    switch (code) {
        case NkIrOp_label:
        case NkIrOp_file:
        case NkIrOp_line:
        case NkIrOp_comment:
            return false;
        default:
            return true;
    }
}

/// @param idx -1u means no padding
static void inspectInstrImpl(NkIrInstr instr, NkStream out, usize idx) {
    char const *opcode_name = NULL;
    if (instr.code == NkIrOp_comment) {
        opcode_name = "//";
    } else {
        opcode_name = s_opcode_names[instr.code];
    }

    if (idx != -1u) {
        if (isAddressableInstr(instr.code)) {
            nk_stream_printf(out, "%5zu |%8s ", idx, opcode_name);
        } else if (instr.code == NkIrOp_comment) {
            nk_stream_printf(out, "%5c |%8s ", ' ', opcode_name);
        }
    } else {
        nk_stream_printf(out, "%s ", opcode_name);
    }

    for (usize i = 1; i < 3; i++) {
        NkIrArg const *arg = &instr.arg[i];
        if (arg->kind != NkIrArg_None && i > 1) {
            nk_stream_printf(out, ", ");
        }

        switch (arg->kind) {
            case NkIrArg_None:
                break;

            case NkIrArg_Ref:
                nkir_inspectRef(arg->ref, out);
                break;

            case NkIrArg_RefArray:
                nk_stream_printf(out, "(");
                for (usize i = 0; i < arg->refs.size; i++) {
                    if (i) {
                        nk_stream_printf(out, ", ");
                    }
                    NkIrRef const *ref = &arg->refs.data[i];
                    nkir_inspectRef(*ref, out);
                }
                nk_stream_printf(out, ")");
                break;

            case NkIrArg_Label:
                nk_stream_printf(out, "@%s", nk_atom2cs(arg->label.name));
                break;

            case NkIrArg_RelLabel:
                nk_stream_printf(out, "<TODO:NkIrArg_RelLabel>");
                break;

            case NkIrArg_Type:
                nk_stream_printf(out, ":");
                nkir_inspectType(arg->type, out);
                break;

            case NkIrArg_String:
                nk_stream_printf(out, NKS_FMT, NKS_ARG(arg->str));
                break;

            case NkIrArg_Idx:
                nk_stream_printf(out, "%" PRIu32, arg->idx);
                break;
        }
    }

    if (instr.arg[0].kind == NkIrArg_Ref && instr.arg[0].ref.kind != NkIrRef_None) {
        nk_stream_printf(out, " -> ");
        nkir_inspectRef(instr.arg[0].ref, out);
    }
}

void nkir_inspectSymbol(NkIrSymbol const *sym, NkStream out) {
    NK_LOG_TRC("%s", __func__);

    switch (sym->vis) {
        case NkIrVisibility_Default:
            nk_stream_printf(out, "pub ");
            break;
        case NkIrVisibility_Local:
            nk_stream_printf(out, "local ");
            break;
        case NkIrVisibility_Hidden: // TODO: Support other visibilities
        case NkIrVisibility_Protected:
        case NkIrVisibility_Internal:
            break;
    }

    if (sym->flags & NkIrSymbol_ThreadLocal) {
        nk_stream_printf(out, "thread_local ");
    }

    switch (sym->kind) {
        case NkIrSymbol_Extern:
            nk_stream_printf(out, "<TODO:NkIrSymbol_Extern>");
            break;

        case NkIrSymbol_Data:
            if (sym->data.flags & NkIrData_ReadOnly) {
                nk_stream_printf(out, "const ");
            } else {
                nk_stream_printf(out, "data ");
            }
            nk_stream_printf(out, "%s: ", nk_atom2cs(sym->name));
            nkir_inspectType(sym->data.type, out);
            if (sym->data.addr) {
                nk_stream_printf(out, " = ");
                nkir_inspectVal(sym->data.addr, sym->data.type, out);
            }
            nk_stream_printf(out, ";");
            break;

        case NkIrSymbol_Proc:
            nk_stream_printf(out, "proc %s(", nk_atom2cs(sym->name));
            for (usize i = 0; i < sym->proc.params.size; i++) {
                if (i) {
                    nk_stream_printf(out, ", ");
                }
                NkIrParam const *param = &sym->proc.params.data[i];
                if (param->name) {
                    nk_stream_printf(out, "%%%s: ", nk_atom2cs(param->name));
                }
                nkir_inspectType(param->type, out);
            }
            nk_stream_printf(out, ") ");
            nkir_inspectType(sym->proc.ret_type, out);
            nk_stream_printf(out, " {\n");
            usize idx = 0;
            for (usize i = 0; i < sym->proc.instrs.size; i++) {
                NkIrInstr const *instr = &sym->proc.instrs.data[i];
                inspectInstrImpl(*instr, out, idx);
                nk_stream_printf(out, "\n");
                if (isAddressableInstr(instr->code)) {
                    idx++;
                }
            }
            nk_stream_printf(out, "}");
            break;
    }

    nk_stream_printf(out, "\n");
}

void nkir_inspectInstr(NkIrInstr instr, NkStream out) {
    NK_LOG_TRC("%s", __func__);

    inspectInstrImpl(instr, out, -1u);
}

void nkir_inspectRef(NkIrRef ref, NkStream out) {
    NK_LOG_TRC("%s", __func__);

    switch (ref.kind) {
        case NkIrRef_None:
            return;

        case NkIrRef_Null:
            break;

        case NkIrRef_Local:
        case NkIrRef_Param:
            nk_stream_printf(out, "%%%s", nk_atom2cs(ref.sym));
            break;

        case NkIrRef_Global:
            nk_stream_printf(out, "%s", nk_atom2cs(ref.sym));
            break;

        case NkIrRef_Imm:
            nkir_inspectVal(&ref.imm, ref.type, out);
            break;

        case NkIrRef_VariadicMarker:
            nk_stream_printf(out, "...");
            return;
    }

    nk_stream_printf(out, ":");
    nkir_inspectType(ref.type, out);
}

bool nkir_validateModule(NkIrModule m) {
    NK_LOG_TRC("%s", __func__);

    (void)m;
    nk_assert(!"TODO: `nkir_validateModule` not implemented");
}

bool nkir_validateProc(NkIrProc const *proc) {
    NK_LOG_TRC("%s", __func__);

    (void)proc;
    nk_assert(!"TODO: `nkir_validateProc` not implemented");
}
