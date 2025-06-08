#include "nkb/ir.h"

#include <llvm-c/Core.h>
#include <llvm-c/Error.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Linker.h>
#include <llvm-c/Transforms/PassBuilder.h>

#include "common.h"
#include "emit_llvm.h"
#include "linker.h"
#include "nkb/types.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

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

static NkIrArg argLabel(NkIrLabel label) {
    switch (label.kind) {
        case NkIrLabel_Abs:
            return (NkIrArg){
                .label = label.name,
                .kind = NkIrArg_Label,
            };
        case NkIrLabel_Rel:
            return (NkIrArg){
                .offset = label.offset,
                .kind = NkIrArg_LabelRel,
            };
    }

    nk_assert(!"unreachable");
    return (NkIrArg){0};
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

static bool isJumpInstr(u8 code) {
    switch (code) {
        case NkIrOp_jmp:
        case NkIrOp_jmpz:
        case NkIrOp_jmpnz:
            return true;
        default:
            return false;
    }
}

void nkir_convertToPic(NkArena *scratch, NkIrInstrArray instrs, NkIrInstrDynArray *out) {
    NK_LOG_TRC("%s", __func__);

    NK_ARENA_SCOPE(scratch) {
        LabelDynArray da_labels = {.alloc = nk_arena_getAllocator(scratch)};
        LabelArray const labels = collectLabels(instrs, &da_labels);

        NK_ITERATE(NkIrInstr const *, instr, instrs) {
            nkda_append(out, *instr);
            NkIrInstr *instr_copy = &nks_last(*out);

            if (isJumpInstr(instr_copy->code)) {
                for (usize ai = 1; ai < 3; ai++) {
                    NkIrArg *arg = &instr_copy->arg[ai];

                    if (arg->kind == NkIrArg_Label) {
                        Label const *label = findLabelByName(labels, arg->label);
                        if (label) {
                            arg->offset = label->idx - NK_INDEX(instr, instrs);
                            arg->kind = NkIrArg_LabelRel;
                            break;
                        }
                    }
                }
            }
        }
    }
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

NkIrLabel nkir_makeLabelAbs(NkAtom name) {
    return (NkIrLabel){
        .name = name,
        .kind = NkIrLabel_Abs,
    };
}

NkIrLabel nkir_makeLabelRel(i32 offset) {
    return (NkIrLabel){
        .offset = offset,
        .kind = NkIrLabel_Rel,
    };
}

NkIrInstr nkir_make_nop() {
    return (NkIrInstr){0};
}

NkIrInstr nkir_make_ret(NkIrRef arg) {
    return (NkIrInstr){
        .arg = {argNull(), argRef(arg), argNull()},
        .code = NkIrOp_ret,
    };
}

NkIrInstr nkir_make_jmp(NkIrLabel label) {
    return (NkIrInstr){
        .arg = {argNull(), argLabel(label), argNull()},
        .code = NkIrOp_jmp,
    };
}

NkIrInstr nkir_make_jmpz(NkIrRef cond, NkIrLabel label) {
    return (NkIrInstr){
        .arg = {argNull(), argRef(cond), argLabel(label)},
        .code = NkIrOp_jmpz,
    };
}

NkIrInstr nkir_make_jmpnz(NkIrRef cond, NkIrLabel label) {
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

NkIrInstr nkir_make_store(NkIrRef dst, NkIrRef src) {
    return (NkIrInstr){
        .arg = {argRef(dst), argRef(src), argNull()},
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
        .arg = {argNull(), argLabel(nkir_makeLabelAbs(label)), argNull()},
        .code = NkIrOp_label,
    };
}

NkIrInstr nkir_make_comment(NkString comment) {
    return (NkIrInstr){
        .arg = {argNull(), argString(comment), argNull()},
        .code = NkIrOp_comment,
    };
}

static void exportModuleImpl(NkArena *scratch, NkIrModule mod, NkString out_file, NkIrOutputKind kind) {
    // TODO: Hardcoded file extensions
    char const *file_ext = "";
    switch (kind) {
        case NkIrOutput_Object:
            file_ext = ".o";
            break;
        case NkIrOutput_Shared:
            file_ext = ".so";
            break;
        case NkIrOutput_Archiv:
            file_ext = ".a";
            break;
        case NkIrOutput_Binary:
        case NkIrOutput_Static:
        case NkIrOutput_None:
            break;
    }
    if (!nks_endsWith(out_file, nk_cs2s(file_ext))) {
        out_file = nk_tsprintf(scratch, NKS_FMT "%s", NKS_ARG(out_file), file_ext);
    }

    // TODO: Generate temporary file more rebustly, and cleanup
    NkString obj_file = kind == NkIrOutput_Object ? nk_tsprintf(scratch, NKS_FMT, NKS_ARG(out_file))
                                                  : nk_tsprintf(scratch, "/tmp/" NKS_FMT ".o", NKS_ARG(out_file));

    NkArena llvm_ir_arena = {0};
    NkStringBuilder llvm_ir = {.alloc = nk_arena_getAllocator(&llvm_ir_arena)};
    nkir_emit_llvm(nksb_getStream(&llvm_ir), scratch, mod);
    nksb_appendNull(&llvm_ir);

    NK_LOG_STREAM_INF {
        NkStream log = nk_log_getStream();
        nk_printf(log, "LLVM IR:\n" NKS_FMT, NKS_ARG(llvm_ir));
    }

    { // TODO: Move LLVM out
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();

        LLVMContextRef context = LLVMContextCreate();
        char *error = NULL;

        LLVMModuleRef module = LLVMModuleCreateWithName("main");

        LLVMMemoryBufferRef buffer = LLVMCreateMemoryBufferWithMemoryRange(llvm_ir.data, llvm_ir.size, "buffer", 0);

        if (LLVMParseIRInContext(context, buffer, &module, &error)) {
            NK_LOG_ERR("Error parsing IR: %s", error);
            LLVMContextDispose(context);
            return;
        }

        char *triple = LLVMGetDefaultTargetTriple();
        LLVMTargetRef target;
        if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
            NK_LOG_ERR("Failed to get target from triple: %s", error);
            LLVMContextDispose(context);
            return;
        }
        LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
            target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocPIC, LLVMCodeModelDefault);

        { // Optimization
            LLVMPassBuilderOptionsRef pbo = LLVMCreatePassBuilderOptions();
            LLVMErrorRef opt_err;
            if ((opt_err = LLVMRunPasses(module, "default<O3>", tm, pbo))) { // TODO: Hardcoded opt level
                char *err_msg = LLVMGetErrorMessage(opt_err);
                NK_LOG_ERR("Failed to optimize: %s", err_msg);
                LLVMDisposeErrorMessage(err_msg);
                LLVMDisposePassBuilderOptions(pbo);
                LLVMDisposeTargetMachine(tm);
                LLVMDisposeMessage(triple);
                LLVMContextDispose(context);
                return;
            }
            LLVMDisposePassBuilderOptions(pbo);
        }

        if (LLVMTargetMachineEmitToFile(tm, module, obj_file.data, LLVMObjectFile, &error) != 0) {
            NK_LOG_ERR("Failed to emit object file: %s", error);
            LLVMDisposeTargetMachine(tm);
            LLVMDisposeMessage(triple);
            LLVMContextDispose(context);
            return;
        }

        LLVMDisposeTargetMachine(tm);
        LLVMDisposeMessage(triple);

        LLVMContextDispose(context);
    }

    // TODO: Use twin scratch arenas
    nk_arena_free(&llvm_ir_arena);

    if (kind != NkIrOutput_None && kind != NkIrOutput_Object) {
        nk_link((NkLikerOpts){
            .scratch = scratch,
            .out_kind = kind,
            .obj_file = obj_file,
            .out_file = out_file,
        });
    }
}

void nkir_exportModule(NkArena *scratch, NkIrModule mod, NkString out_file, NkIrOutputKind kind) {
    NK_LOG_TRC("%s", __func__);

    NK_ARENA_SCOPE(scratch) {
        exportModuleImpl(scratch, mod, out_file, kind);
    }
}

NkIrRunCtx nkir_createRunCtx(NkIrModule *mod, NkArena *arena) {
    NK_LOG_TRC("%s", __func__);

    (void)mod;
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

bool nkir_run(NkIrModule mod) {
    NkArena scratch = {0};
    NkArena llvm_ir_arena = {0};
    NkStringBuilder llvm_ir = {.alloc = nk_arena_getAllocator(&llvm_ir_arena)};
    nkir_emit_llvm(nksb_getStream(&llvm_ir), &scratch, mod);
    nksb_appendNull(&llvm_ir);

    NK_LOG_STREAM_INF {
        NkStream log = nk_log_getStream();
        nk_printf(log, "LLVM IR:\n" NKS_FMT, NKS_ARG(llvm_ir));
    }

    { // TODO: Move LLVM out
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();
        LLVMLinkInMCJIT();

        LLVMContextRef context = LLVMContextCreate();
        char *error = NULL;

        LLVMModuleRef module = LLVMModuleCreateWithName("main");

        LLVMExecutionEngineRef engine;
        if (LLVMCreateExecutionEngineForModule(&engine, module, &error)) {
            NK_LOG_ERR("Failed creating execution engine: %s", error);
            LLVMContextDispose(context);
            return false;
        }

        const char *triple = LLVMGetTarget(module);
        const char *dataLayout = LLVMGetDataLayoutStr(module);

        {
            LLVMMemoryBufferRef buffer = LLVMCreateMemoryBufferWithMemoryRange(llvm_ir.data, llvm_ir.size, "buffer", 0);

            LLVMModuleRef temp_module = NULL;
            if (LLVMParseIRInContext(context, buffer, &temp_module, &error)) {
                NK_LOG_ERR("Error parsing IR: %s", error);
                LLVMContextDispose(context);
                return false;
            }
            LLVMSetTarget(temp_module, triple);
            LLVMSetDataLayout(temp_module, dataLayout);

            { // Optimization
                char *triple = LLVMGetDefaultTargetTriple();
                LLVMTargetRef target;
                if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
                    NK_LOG_ERR("Failed to get target from triple: %s", error);
                    LLVMDisposeMessage(error);
                    LLVMDisposeModule(temp_module);
                    LLVMDisposeExecutionEngine(engine);
                    LLVMContextDispose(context);
                    return false;
                }
                LLVMTargetMachineRef tm = LLVMCreateTargetMachine(
                    target, triple, "generic", "", LLVMCodeGenLevelDefault, LLVMRelocDefault, LLVMCodeModelJITDefault);

                LLVMPassBuilderOptionsRef pbo = LLVMCreatePassBuilderOptions();
                LLVMErrorRef opt_err;
                if ((opt_err = LLVMRunPasses(temp_module, "default<O3>", tm, pbo))) { // TODO: Hardcoded opt level
                    char *err_msg = LLVMGetErrorMessage(opt_err);
                    NK_LOG_ERR("Failed to optimize: %s", err_msg);
                    LLVMDisposeErrorMessage(err_msg);
                    LLVMDisposePassBuilderOptions(pbo);
                    LLVMDisposeTargetMachine(tm);
                    LLVMDisposeMessage(triple);
                    LLVMDisposeExecutionEngine(engine);
                    LLVMContextDispose(context);
                    return false;
                }

                LLVMDisposeMessage(triple);
                LLVMDisposePassBuilderOptions(pbo);
                LLVMDisposeTargetMachine(tm);
            }

            if (LLVMLinkModules2(module, temp_module)) {
                NK_LOG_ERR("Failed to link module\n");
                LLVMDisposeExecutionEngine(engine);
                LLVMContextDispose(context);
                return false;
            }
        }

        LLVMValueRef entry_func = LLVMGetNamedFunction(module, "_entry");
        if (!entry_func) {
            NK_LOG_ERR("Failed to find `_entry`");
            LLVMDisposeExecutionEngine(engine);
            LLVMContextDispose(context);
            return false;
        }

        void (*entry)() = LLVMGetPointerToGlobal(engine, entry_func);
        if (!entry) {
            NK_LOG_ERR("`_entry` is NULL");
            LLVMDisposeExecutionEngine(engine);
            LLVMContextDispose(context);
            return false;
        }

        entry();

        LLVMDisposeExecutionEngine(engine);
        LLVMContextDispose(context);
    }

    // TODO: Use twin scratch arenas
    nk_arena_free(&llvm_ir_arena);
    nk_arena_free(&scratch);

    return true;
}

void nkir_inspectModule(NkStream out, NkArena *scratch, NkIrModule mod) {
    NK_ITERATE(NkIrSymbol const *, sym, mod) {
        nk_printf(out, "\n");
        nkir_inspectSymbol(out, scratch, sym);
    }
}

static char const *s_opcode_names[] = {
#define IR(NAME) #NAME,
#define DBL_IR(NAME1, NAME2) #NAME1 " " #NAME2,
#include "nkb/ir.inl"
};

static void inspectLabel(NkStream out, Label const *label, LabelArray labels, u32 const *indices) {
    u32 const label_idx = indices[NK_INDEX(label, labels)];
    if (label_idx) {
        nk_printf(out, "@%s%u", nk_atom2cs(label->name), label_idx);
    } else {
        nk_printf(out, "@%s", nk_atom2cs(label->name));
    }
}

typedef struct {
    NkIrInstrArray instrs;
    LabelArray labels;
    u32 const *indices;
} InspectInstrCtx;

static void inspectInstrImpl(NkStream out, usize idx, InspectInstrCtx ctx) {
    if (idx >= ctx.instrs.size) {
        nk_printf(out, "instr@%zu", idx);
        return;
    }

    NkIrInstr const *instr = &ctx.instrs.data[idx];

    if (instr->code == NkIrOp_label) {
    } else if (instr->code == NkIrOp_comment) {
        nk_printf(out, "%5s | ", "//");
    } else {
        nk_printf(out, "%5zu |%7s ", idx, s_opcode_names[instr->code]);
    }

    for (usize ai = 0; ai < 3; ai++) {
        usize const arg_idx = (ai + 1) % 3;

        NkIrArg const *arg = &instr->arg[arg_idx];

        if (arg->kind != NkIrArg_None) {
            if (arg_idx == 0) {
                nk_printf(out, " -> ");
            } else if (arg_idx == 2) {
                nk_printf(out, ", ");
            }
        }

        switch (arg->kind) {
            case NkIrArg_None:
                break;

            case NkIrArg_Ref:
                nkir_inspectRef(out, arg->ref);
                break;

            case NkIrArg_RefArray:
                nk_printf(out, "(");
                NK_ITERATE(NkIrRef const *, ref, arg->refs) {
                    if (NK_INDEX(ref, arg->refs)) {
                        nk_printf(out, ", ");
                    }
                    nkir_inspectRef(out, *ref);
                }
                nk_printf(out, ")");
                break;

            case NkIrArg_Label: {
                Label const *label = instr->code == NkIrOp_label ? findLabelByIdx(ctx.labels, idx)
                                                                 : findLabelByName(ctx.labels, arg->label);
                if (label) {
                    inspectLabel(out, label, ctx.labels, ctx.indices);
                } else {
                    nk_printf(out, "@%s", nk_atom2cs(arg->label));
                }
                break;
            }

            case NkIrArg_LabelRel: {
                usize const target_idx = idx + arg->offset;
                Label const *label = findLabelByIdx(ctx.labels, target_idx);
                if (label) {
                    inspectLabel(out, label, ctx.labels, ctx.indices);
                } else {
                    nk_printf(out, "@%s%i", arg->offset >= 0 ? "+" : "", arg->offset);
                }
                break;
            }

            case NkIrArg_Type:
                nk_printf(out, ":");
                nkir_inspectType(arg->type, out);
                break;

            case NkIrArg_String:
                nk_printf(out, NKS_FMT, NKS_ARG(arg->str));
                break;
        }
    }
}

static void inspectVal(NkStream out, void *base_addr, usize base_offset, NkIrRelocArray relocs, NkIrType type) {
    if (!base_addr) {
        nk_printf(out, "(null)");
        return;
    }
    switch (type->kind) {
        case NkIrType_Aggregate:
            nk_printf(out, "{");
            NK_ITERATE(NkIrAggregateElemInfo const *, elem, type->aggr) {
                if (NK_INDEX(elem, type->aggr)) {
                    nk_printf(out, ", ");
                }
                usize offset = base_offset + elem->offset;
                if (elem->type->kind == NkIrType_Numeric && elem->type->size == 1) {
                    char const *addr = (char *)base_addr + offset;
                    nk_printf(out, "\"");
                    nks_escape(out, (NkString){addr, elem->count});
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
                                nk_printf(out, "$%s", nk_atom2cs(reloc->sym));
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

void nkir_inspectSymbol(NkStream out, NkArena *scratch, NkIrSymbol const *sym) {
    switch (sym->vis) {
        case NkIrVisibility_Default:
            nk_printf(out, "pub ");
            break;
        case NkIrVisibility_Local:
            nk_printf(out, "local ");
            break;
        case NkIrVisibility_Hidden: // TODO: Support other visibilities
        case NkIrVisibility_Protected:
        case NkIrVisibility_Internal:
            break;
    }

    if (sym->flags & NkIrSymbol_ThreadLocal) {
        nk_printf(out, "thread_local ");
    }

    NkString const sym_str = nk_atom2s(sym->name);

    switch (sym->kind) {
        case NkIrSymbol_Proc: {
            LabelDynArray da_labels = {.alloc = nk_arena_getAllocator(scratch)};
            LabelArray const labels = collectLabels(sym->proc.instrs, &da_labels);

            u32 *indices = countLabels(scratch, labels);

            if (sym_str.size) {
                nk_printf(out, "proc $" NKS_FMT "(", NKS_ARG(sym_str));
            } else {
                nk_printf(out, "proc $_%" PRIu32 "(", sym->name);
            }
            NK_ITERATE(NkIrParam const *, param, sym->proc.params) {
                if (NK_INDEX(param, sym->proc.params)) {
                    nk_printf(out, ", ");
                }
                nk_printf(out, ":");
                nkir_inspectType(param->type, out);
                if (param->name) {
                    nk_printf(out, " %%%s", nk_atom2cs(param->name));
                }
            }
            nk_printf(out, ") :");
            nkir_inspectType(sym->proc.ret.type, out);
            if (sym->proc.ret.name) {
                nk_printf(out, " %%%s", nk_atom2cs(sym->proc.ret.name));
            }
            nk_printf(out, " {\n");
            NK_ITERATE(NkIrInstr const *, instr, sym->proc.instrs) {
                inspectInstrImpl(
                    out,
                    NK_INDEX(instr, sym->proc.instrs),
                    (InspectInstrCtx){
                        .instrs = sym->proc.instrs,
                        .labels = labels,
                        .indices = indices,
                    });
                nk_printf(out, "\n");
            }
            nk_printf(out, "}");
            break;
        }

        case NkIrSymbol_Data:
            if (sym->data.flags & NkIrData_ReadOnly) {
                nk_printf(out, "const ");
            } else {
                nk_printf(out, "data ");
            }
            // TODO: Inline strings
            if (sym_str.size) {
                nk_printf(out, "$" NKS_FMT " :", NKS_ARG(sym_str));
            } else {
                nk_printf(out, "$_%" PRIu32 " :", sym->name);
            }
            nkir_inspectType(sym->data.type, out);
            if (sym->data.addr) {
                nk_printf(out, " ");
                inspectVal(out, sym->data.addr, 0, sym->data.relocs, sym->data.type);
            }
            break;

        case NkIrSymbol_ExternProc:
            nk_printf(out, "extern ");
            if (sym->extern_proc.lib) {
                nk_printf(out, "\"");
                nks_escape(out, nk_atom2s(sym->extern_proc.lib));
                nk_printf(out, "\" ");
            }
            nk_printf(out, "proc $" NKS_FMT "(", NKS_ARG(sym_str));
            NK_ITERATE(NkIrType const *, type, sym->extern_proc.param_types) {
                if (NK_INDEX(type, sym->extern_proc.param_types)) {
                    nk_printf(out, ", ");
                }
                nkir_inspectType(*type, out);
            }
            if (sym->extern_proc.flags & NkIrProc_Variadic) {
                nk_printf(out, ", ...");
            }
            nk_printf(out, ") :");
            nkir_inspectType(sym->extern_proc.ret_type, out);
            break;

        case NkIrSymbol_ExternData:
            nk_printf(out, "extern ");
            if (sym->extern_data.lib) {
                nk_printf(out, "\"");
                nks_escape(out, nk_atom2s(sym->extern_data.lib));
                nk_printf(out, "\" ");
            }
            nk_printf(out, "data $" NKS_FMT " :", NKS_ARG(sym_str));
            nkir_inspectType(sym->extern_data.type, out);
            break;
    }

    nk_printf(out, "\n");
}

void nkir_inspectInstr(NkStream out, NkIrInstr instr) {
    inspectInstrImpl(
        out,
        0,
        (InspectInstrCtx){
            .instrs = (NkIrInstrArray){&instr, 1},
        });
}

void nkir_inspectRef(NkStream out, NkIrRef ref) {
    if (ref.kind == NkIrRef_None) {
        return;
    }

    if (ref.kind == NkIrRef_VariadicMarker) {
        nk_printf(out, "...");
        return;
    }

    nk_printf(out, ":");
    nkir_inspectType(ref.type, out);

    switch (ref.kind) {
        case NkIrRef_Local:
        case NkIrRef_Param:
            nk_printf(out, " %%%s", nk_atom2cs(ref.sym));
            break;

        case NkIrRef_Global: {
            NkString const sym_str = nk_atom2s(ref.sym);
            if (sym_str.size) {
                nk_printf(out, " $" NKS_FMT, NKS_ARG(sym_str));
            } else {
                nk_printf(out, " $_%" PRIu32, ref.sym);
            }
            break;
        }

        case NkIrRef_Imm:
            nk_printf(out, " ");
            nkir_inspectVal(&ref.imm, ref.type, out);
            break;

        case NkIrRef_None:
        case NkIrRef_Null:
        case NkIrRef_VariadicMarker:
            break;
    }
}

bool nkir_validateModule(NkIrModule mod) {
    NK_LOG_TRC("%s", __func__);

    (void)mod;
    nk_assert(!"TODO: `nkir_validateModule` not implemented");
}

bool nkir_validateProc(NkIrProc const *proc) {
    NK_LOG_TRC("%s", __func__);

    (void)proc;
    nk_assert(!"TODO: `nkir_validateProc` not implemented");
}
