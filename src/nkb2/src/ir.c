#include "nkb/ir.h"

#include <unistd.h> // TODO: Remove, only used for _exit

#include "common.h"
#include "linker.h"
#include "llvm_adapter.h"
#include "nkb/types.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/hash_tree.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/stream.h"
#include "ntk/string.h"
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

typedef struct NkbState_T {
    NkArena arena;
    NkArena scratch;

    NkLlvmState llvm;
    NkLlvmJitState _llvm_jit;

    NkDynArray(NkLlvmTarget) created_targets;
} NkbState_T;

typedef struct NkIrModule_T {
    NkbState nkb;
    NkIrSymbolDynArray syms;

    NkAtomSet rt_loaded_syms;

    NkIrSymbolResolver sym_resolver_fn;
    void *sym_resolver_userdata;

    NkLlvmJitDylib _llvm_jit_dylib;
} NkIrModule_T;

NkbState nkir_createState(void) {
    NK_LOG_TRC("%s", __func__);

    NkArena arena = {0};
    NkbState nkb = nk_arena_allocT(&arena, NkbState_T);
    *nkb = (NkbState_T){
        .arena = arena,
    };
    nkb->llvm = nk_llvm_createState(&nkb->arena);
    nkb->created_targets.alloc = nk_arena_getAllocator(&nkb->arena);

    return nkb;
}

void nkir_freeState(NkbState nkb) {
    NK_LOG_TRC("%s", __func__);

    NK_ITERATE(NkLlvmTarget const *, it, nkb->created_targets) {
        nk_llvm_freeTarget(*it);
    }

    nk_llvm_freeJitState(nkb->_llvm_jit);
    nk_llvm_freeState(nkb->llvm);

    nk_arena_free(&nkb->scratch);

    NkArena arena = nkb->arena;
    nk_arena_free(&arena);
}

NkIrModule nkir_createModule(NkbState nkb) {
    NkIrModule mod = nk_arena_allocT(&nkb->arena, NkIrModule_T);
    *mod = (NkIrModule_T){
        .nkb = nkb,
        .syms = {.alloc = nk_arena_getAllocator(&nkb->arena)},

        .rt_loaded_syms = {.alloc = nk_arena_getAllocator(&nkb->arena)},
    };
    return mod;
}

NkIrTarget nkir_createTarget(NkbState nkb, NkString triple) {
    NK_LOG_TRC("%s", __func__);

    NkLlvmTarget tgt = NULL;
    NK_ARENA_SCOPE(&nkb->scratch) {
        tgt = nk_llvm_createTarget(nkb->llvm, nk_tprintf(&nkb->scratch, NKS_FMT, NKS_ARG(triple)));
    }
    if (tgt) {
        nkda_append(&nkb->created_targets, tgt);
    }
    return (NkIrTarget)tgt;
}

NkArena *nkir_moduleGetArena(NkIrModule mod) {
    return &mod->nkb->arena;
}

void nkir_moduleDefineSymbol(NkIrModule mod, NkIrSymbol const *sym) {
    nkda_append(&mod->syms, *sym);
}

NkIrRefDynArray nkir_moduleNewRefArray(NkIrModule mod) {
    return (NkIrRefDynArray){.alloc = nk_arena_getAllocator(&mod->nkb->arena)};
}

NkIrInstrDynArray nkir_moduleNewInstrArray(NkIrModule mod) {
    return (NkIrInstrDynArray){.alloc = nk_arena_getAllocator(&mod->nkb->arena)};
}

NkIrTypeDynArray nkir_moduleNewTypeArray(NkIrModule mod) {
    return (NkIrTypeDynArray){.alloc = nk_arena_getAllocator(&mod->nkb->arena)};
}

NkIrParamDynArray nkir_moduleNewParamArray(NkIrModule mod) {
    return (NkIrParamDynArray){.alloc = nk_arena_getAllocator(&mod->nkb->arena)};
}

NkIrRelocDynArray nkir_moduleNewRelocArray(NkIrModule mod) {
    return (NkIrRelocDynArray){.alloc = nk_arena_getAllocator(&mod->nkb->arena)};
}

void nkir_setSymbolResolver(NkIrModule mod, NkIrSymbolResolver fn, void *userdata) {
    nk_assert(!mod->sym_resolver_fn && "overwriting existing symbol resolver");

    mod->sym_resolver_fn = fn;
    mod->sym_resolver_userdata = userdata;
}

NkIrSymbolArray nkir_moduleGetSymbols(NkIrModule mod) {
    return (NkIrSymbolArray){NKS_INIT(mod->syms)};
}

NkIrSymbol const *nkir_findSymbol(NkIrModule mod, NkAtom sym) {
    NK_ITERATE(NkIrSymbol *, it, mod->syms) {
        if (it->name == sym) {
            return it;
        }
    }
    return NULL;
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

static bool exportModuleImpl(
    NkArena *scratch,
    NkIrModule mod,
    NkIrTarget target,
    NkString out_file,
    NkIrOutputKind kind) {
    NK_LOG_TRC("%s", __func__);

    NkbState nkb = mod->nkb;

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

    NkLlvmTarget tgt = (NkLlvmTarget)target;

    NkLlvmModule llvm_mod = nk_llvm_compilerIr(scratch, nkb->llvm, (NkIrSymbolArray){NKS_INIT(mod->syms)});
    nk_llvm_optimizeIr(scratch, llvm_mod, tgt, NkLlvmOptLevel_O3); // TODO: Hardcoded opt level

    nk_llvm_emitObjectFile(llvm_mod, tgt, obj_file);

    if (kind != NkIrOutput_None && kind != NkIrOutput_Object) {
        nk_link((NkLikerOpts){
            .scratch = scratch,
            .out_kind = kind,
            .obj_file = obj_file,
            .out_file = out_file,
        });
    }

    return true;
}

bool nkir_exportModule(NkIrModule mod, NkIrTarget target, NkString out_file, NkIrOutputKind kind) {
    bool ret = false;
    NkbState nkb = mod->nkb;
    NkArena *scratch = &nkb->scratch;
    NK_ARENA_SCOPE(scratch) {
        ret = exportModuleImpl(scratch, mod, target, out_file, kind);
    }
    return ret;
}

bool nkir_invoke(NkIrModule mod, NkAtom sym, void **args, void **ret) {
    NK_LOG_TRC("%s", __func__);

    (void)mod;
    (void)sym;
    (void)args;
    (void)ret;
    nk_assert(!"TODO: `nkir_invoke` not implemented");
    return false;
}

static NkLlvmJitState getLlvmJitState(NkbState nkb) {
    if (!nkb->_llvm_jit) {
        nkb->_llvm_jit = nk_llvm_createJitState(nkb->llvm);
    }
    return nkb->_llvm_jit;
}

static NkLlvmJitDylib getLlvmJitDylib(NkIrModule mod) {
    if (!mod->_llvm_jit_dylib) {
        NkbState nkb = mod->nkb;
        mod->_llvm_jit_dylib = nk_llvm_createJitDylib(nkb->llvm, getLlvmJitState(nkb));
    }
    return mod->_llvm_jit_dylib;
}

static void collectSymbols(NkIrModule mod, _NkAtomSet_Node *node, NkIrSymbolDynArray *out) {
    if (!node) {
        return;
    }

    NkIrSymbol const *sym = nkir_findSymbol(mod, node->item);
    nk_assert(sym && "symbol not found, invalid ir");
    nkda_append(out, *sym);

    collectSymbols(mod, node->child[0], out);
    collectSymbols(mod, node->child[1], out);
}

typedef NkDynArray(NkAtom) NkAtomDynArray;

static void gatherDeps(NkIrSymbol const *sym, NkAtomDynArray *out) {
    switch (sym->kind) {
        case NkIrSymbol_Proc:
            NK_ITERATE(NkIrInstr const *, instr, sym->proc.instrs) {
                for (usize i = 0; i < NK_ARRAY_COUNT(instr->arg); i++) {
                    NkIrArg const *arg = &instr->arg[i];
                    switch (arg->kind) {
                        case NkIrArg_Ref:
                            if (arg->ref.kind == NkIrRef_Global) {
                                nkda_append(out, arg->ref.sym);
                            }
                            break;

                        case NkIrArg_RefArray:
                            NK_ITERATE(NkIrRef const *, ref, arg->refs) {
                                if (ref->kind == NkIrRef_Global) {
                                    nkda_append(out, ref->sym);
                                }
                            }
                            break;

                        case NkIrArg_None:
                        case NkIrArg_Label:
                        case NkIrArg_LabelRel:
                        case NkIrArg_Type:
                        case NkIrArg_String:
                            break;
                    }
                }
            }
            break;

        case NkIrSymbol_Data:
            NK_ITERATE(NkIrReloc const *, reloc, sym->data.relocs) {
                nkda_append(out, reloc->sym);
            }
            break;

        case NkIrSymbol_None:
        case NkIrSymbol_Extern:
            break;
    }
}

static void getSymbolDependencies(NkIrModule mod, NkAtom sym_name, NkIrSymbolDynArray *out) {
    NK_LOG_TRC("%s", __func__);

    NK_PROF_FUNC() {
        NK_LOG_DBG("Getting dependencies for `%s`", nk_atom2cs(sym_name));

        NkbState nkb = mod->nkb;
        NkArena *scratch = &nkb->scratch;

        NkAtomDynArray stack = {.alloc = nk_arena_getAllocator(scratch)};
        NkAtomSet deps = {.alloc = nk_arena_getAllocator(scratch)};

        nkda_append(&stack, sym_name);

        while (stack.size) {
            NkAtom const sym_name = nks_last(stack);
            nkda_pop(&stack, 1);

            if (!NkAtomSet_find(&deps, sym_name)) {
                NkAtomSet_insert(&deps, sym_name);

                NkIrSymbol const *sym = nkir_findSymbol(mod, sym_name);
                nk_assert(sym && "symbol not found, invalid ir");
                gatherDeps(sym, &stack);
            }
        }

        collectSymbols(mod, deps.root, out);
    }
}

static NkIrSymbol symToExtern(NkArena *arena, NkIrSymbol sym) {
    switch (sym.kind) {
        case NkIrSymbol_Proc: {
            NkIrTypeDynArray param_types = {.alloc = nk_arena_getAllocator(arena)};
            NK_ITERATE(NkIrParam const *, param, sym.proc.params) {
                nkda_append(&param_types, param->type);
            }

            return (NkIrSymbol){
                .extrn =
                    {
                        .proc =
                            {
                                .param_types = {NKS_INIT(param_types)},
                                .ret_type = sym.proc.ret.type,
                                .flags = sym.proc.flags,
                            },
                        .kind = NkIrExtern_Proc,
                    },
                .name = sym.name,
                .vis = sym.vis,
                .flags = sym.flags,
                .kind = NkIrSymbol_Extern,
            };
        }

        case NkIrSymbol_Data:
            return (NkIrSymbol){
                .extrn =
                    {
                        .data =
                            {
                                .type = sym.data.type,
                            },
                        .kind = NkIrExtern_Data,
                    },
                .name = sym.name,
                .vis = sym.vis,
                .flags = sym.flags,
                .kind = NkIrSymbol_Extern,
            };

        case NkIrSymbol_None:
        case NkIrSymbol_Extern:
            break;
    }

    return sym;
}

static void *getSymbolAddressImpl(NkArena *scratch, NkIrModule mod, NkAtom sym_name) {
    NkbState nkb = mod->nkb;

    NkIrSymbolDynArray syms = {.alloc = nk_arena_getAllocator(scratch)};
    getSymbolDependencies(mod, sym_name, &syms);

    NkIrSymbolAddressDynArray to_define = {.alloc = nk_arena_getAllocator(scratch)};

    NK_ITERATE(NkIrSymbol *, sym, syms) {
        if (NkAtomSet_find(&mod->rt_loaded_syms, sym->name)) {
            if (sym->kind == NkIrSymbol_Proc || sym->kind == NkIrSymbol_Data) {
                *sym = symToExtern(scratch, *sym);
            }
        } else {
            NkAtomSet_insert(&mod->rt_loaded_syms, sym->name);

            if (sym->kind == NkIrSymbol_Extern) {
                if (!mod->sym_resolver_fn) {
                    // TODO: Report errors properly
                    NK_LOG_ERR("Symbol resolver is not set up");
                    _exit(1);
                }

                void *addr = mod->sym_resolver_fn(sym->name, mod->sym_resolver_userdata);
                if (!addr) {
                    // TODO: Report errors properly
                    NK_LOG_ERR("Failed to resolve symbol `%s`", nk_atom2cs(sym->name));
                    _exit(1);
                }

                nkda_append(
                    &to_define,
                    ((NkIrSymbolAddress){
                        .sym = sym->name,
                        .addr = addr,
                    }));
            }
        }
    }

    NkLlvmJitState jit = getLlvmJitState(nkb);
    NkLlvmJitDylib jdl = getLlvmJitDylib(mod);

    nk_llvm_defineExternSymbols(scratch, jit, jdl, (NkIrSymbolAddressArray){NKS_INIT(to_define)});

    NkLlvmModule llvm_mod = nk_llvm_compilerIr(scratch, nkb->llvm, (NkIrSymbolArray){NKS_INIT(syms)});

    NkLlvmTarget tgt = nk_llvm_getJitTarget(jit);
    nk_llvm_optimizeIr(scratch, llvm_mod, tgt, NkLlvmOptLevel_O3); // TODO: Hardcoded opt level

    nk_llvm_jitModule(llvm_mod, jit, jdl);

    return nk_llvm_getSymbolAddress(jit, jdl, sym_name);
}

void *nkir_getSymbolAddress(NkIrModule mod, NkAtom sym) {
    NK_LOG_TRC("%s", __func__);

    NkbState nkb = mod->nkb;
    NkArena *scratch = &nkb->scratch;

    if (!nkir_findSymbol(mod, sym)) {
        // TODO: Report errors properly
        NK_LOG_ERR("Symbol not found: %s", nk_atom2cs(sym));
        _exit(1);
    }

    void *addr = NULL;
    NK_PROF_FUNC() {
        NK_ARENA_SCOPE(scratch) {
            addr = getSymbolAddressImpl(scratch, mod, sym);
        }
    }

    return addr;
}

bool nkir_defineExternSymbols(NkIrModule mod, NkIrSymbolAddressArray syms) {
    NK_LOG_TRC("%s", __func__);

    NkbState nkb = mod->nkb;
    NkArena *scratch = &nkb->scratch;

    NK_ARENA_SCOPE(scratch) {
        nk_llvm_defineExternSymbols(scratch, getLlvmJitState(nkb), getLlvmJitDylib(mod), syms);
    }

    return true;
}

void nkir_printName(NkStream out, char const *kind, NkAtom name) {
    NkString const str = nk_atom2s(name);
    if (str.size) {
        nk_printf(out, NKS_FMT, NKS_ARG(str));
    } else {
        nk_printf(out, "__nkl_%s_%u__", kind, name);
    }
}

void nkir_printSymbolName(NkStream out, NkAtom sym) {
    nkir_printName(out, "anonymous", sym);
}

void nkir_inspectModule(NkStream out, NkArena *scratch, NkIrModule mod) {
    NK_ITERATE(NkIrSymbol const *, sym, mod->syms) {
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
                            if (reloc->offset == offset && elem->type->kind == NkIrType_Numeric) {
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
        case NkIrVisibility_Unknown:
        case NkIrVisibility_Hidden: // TODO: Support other visibilities
        case NkIrVisibility_Protected:
        case NkIrVisibility_Internal:
            break;
    }

    if (sym->flags & NkIrSymbol_ThreadLocal) {
        nk_printf(out, "thread_local ");
    }

    switch (sym->kind) {
        case NkIrSymbol_None:
            break;

        case NkIrSymbol_Proc: {
            LabelDynArray da_labels = {.alloc = nk_arena_getAllocator(scratch)};
            LabelArray const labels = collectLabels(sym->proc.instrs, &da_labels);

            u32 *indices = countLabels(scratch, labels);

            nk_printf(out, "proc $");
            nkir_printSymbolName(out, sym->name);
            nk_printf(out, "(");
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
            nk_printf(out, "$");
            nkir_printSymbolName(out, sym->name);
            nk_printf(out, " :");
            nkir_inspectType(sym->data.type, out);
            if (sym->data.addr) {
                nk_printf(out, " ");
                inspectVal(out, sym->data.addr, 0, sym->data.relocs, sym->data.type);
            }
            break;

        case NkIrSymbol_Extern:
            nk_printf(out, "extern ");
            if (sym->extrn.lib) {
                nk_printf(out, "\"");
                nks_escape(out, nk_atom2s(sym->extrn.lib));
                nk_printf(out, "\" ");
            }
            switch (sym->extrn.kind) {
                case NkIrExtern_Proc:
                    nk_printf(out, "proc $");
                    nkir_printSymbolName(out, sym->name);
                    nk_printf(out, "(");
                    NK_ITERATE(NkIrType const *, type, sym->extrn.proc.param_types) {
                        if (NK_INDEX(type, sym->extrn.proc.param_types)) {
                            nk_printf(out, ", ");
                        }
                        nk_printf(out, ":");
                        nkir_inspectType(*type, out);
                    }
                    if (sym->extrn.proc.flags & NkIrProc_Variadic) {
                        nk_printf(out, ", ...");
                    }
                    nk_printf(out, ") :");
                    nkir_inspectType(sym->extrn.proc.ret_type, out);
                    break;

                case NkIrExtern_Data:
                    nk_printf(out, "data $");
                    nkir_printSymbolName(out, sym->name);
                    nk_printf(out, " :");
                    nkir_inspectType(sym->extrn.data.type, out);
                    break;
            }
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
            nk_printf(out, " $");
            nkir_printSymbolName(out, ref.sym);
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
    return false;
}

bool nkir_validateProc(NkIrProc const *proc) {
    NK_LOG_TRC("%s", __func__);

    (void)proc;
    nk_assert(!"TODO: `nkir_validateProc` not implemented");
    return false;
}
