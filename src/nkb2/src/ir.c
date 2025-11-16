#include "nkb/ir.h"

#include "common.h"
#include "linker.h"
#include "llvm_adapter.h"
#include "nkb/types.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/error.h"
#include "ntk/hash_tree_array.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(ir);

#define TRY(EXPR, ...)          \
    do {                        \
        if (!(EXPR)) {          \
            return __VA_ARGS__; \
        }                       \
    } while (0)

static NkbState nkb_wrap(NkLlvmState val) {
    return (NkbState)val;
}
static NkLlvmState nkb_unwrap(NkbState val) {
    return (NkLlvmState)val;
}

static NkIrTarget tgt_wrap(NkLlvmTarget val) {
    return (NkIrTarget)val;
}
static NkLlvmTarget tgt_unwrap(NkIrTarget val) {
    return (NkLlvmTarget)val;
}

static NkIrRuntime rt_wrap(NkLlvmJitState val) {
    return (NkIrRuntime)val;
}
static NkLlvmJitState rt_unwrap(NkIrRuntime val) {
    return (NkLlvmJitState)val;
}

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

static NkIrArg argPhiArgsArray(NkIrPhiArgArray args) {
    return (NkIrArg){
        .phi_args = args,
        .kind = NkIrArg_PhiArgsArray,
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

NkAtom NkIrSymbol_getKey(NkIrSymbol const *item) {
    return item->name;
}

NK_HASH_TREE_ARRAY_IMPL(NkIrSymbolDynArray, NkIrSymbol, NkAtom, NkIrSymbol_getKey, nk_atom_hash, nk_atom_equal);

typedef struct NkIrDylib_T {
    NkIrRuntime rt;
    NkIrModule mod;

    NkAtomSet rt_loaded_syms;

    NkIrSymbolResolver sym_resolver_fn;
    void *sym_resolver_userdata;

    NkLlvmJitDylib llvm_jit_dylib;
} NkIrDylib_T;

NkbState nkir_createState(NkArena *arena) {
    NK_LOG_TRC("%s", __func__);

    return nkb_wrap(nk_llvm_createState(arena));
}

void nkir_freeState(NkbState nkb) {
    NK_LOG_TRC("%s", __func__);

    TRY(nkb);

    nk_llvm_freeState(nkb_unwrap(nkb));
}

NkIrTarget nkir_createTarget(NkbState nkb, NkString triple) {
    NK_LOG_TRC("%s", __func__);

    TRY(nkb, NULL);

    NkLlvmTarget tgt = NULL;

    NkArena *scratch = nk_arena_getScratch(NULL);
    NK_ARENA_SCOPE(scratch) {
        tgt = nk_llvm_createTarget(nkb_unwrap(nkb), nk_tprintf(scratch, NKS_FMT, NKS_ARG(triple)));
    }

    return tgt_wrap(tgt);
}

void nkir_freeTarget(NkIrTarget tgt) {
    nk_llvm_freeTarget(tgt_unwrap(tgt));
}

void nkir_convertToPic(NkIrInstrArray instrs, NkIrInstrDynArray *out) {
    NK_LOG_TRC("%s", __func__);

    TRY(out);

    NkArena *scratch = nk_arena_getScratch(nk_arena_getOptArenaFromAllocator(out->alloc));
    NK_ARENA_SCOPE(scratch) {
        LabelDynArray da_labels = {.alloc = nk_arena_getAllocator(scratch)};
        LabelArray const labels = collectLabels(instrs, &da_labels);

        NK_ITERATE(NkIrInstr const *, instr, instrs) {
            nkda_append(out, *instr);
            NkIrInstr *instr_copy = &NKS_LAST(*out);

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

NkIrRef nkir_makeVariadicMarker(void) {
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

NkIrInstr nkir_make_nop(void) {
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

NkIrInstr nkir_make_offset(NkIrRef dst, NkIrRef ptr, NkIrRef idx) {
    return (NkIrInstr){
        .arg = {argRef(dst), argRef(ptr), argRef(idx)},
        .code = NkIrOp_offset,
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

NkIrInstr nkir_make_phi(NkIrRef dst, NkIrPhiArgArray args) {
    return (NkIrInstr){
        .arg = {argRef(dst), argPhiArgsArray(args), argNull()},
        .code = NkIrOp_phi,
    };
}

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
    NkbState nkb,
    NkIrModule mod,
    NkIrTarget tgt,
    NkString out_file,
    NkIrOutputKind kind) {
    NK_LOG_TRC("%s", __func__);

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

    char tmp_path[NK_MAX_PATH];
    if (nk_getTempPath(tmp_path, sizeof(tmp_path)) < 0) {
        nk_error_printf("Failed to get temporary directory path: %s", nk_getLastErrorString());
        return false;
    }

    // TODO: Generate temporary file more rebustly, and cleanup
    NkString obj_file = kind == NkIrOutput_Object
                            ? nk_tsprintf(scratch, NKS_FMT, NKS_ARG(out_file))
                            : nk_tsprintf(scratch, "%s" NKS_FMT ".o", tmp_path, NKS_ARG(out_file));

    NkLlvmModule llvm_mod = nk_llvm_compileIr(nkb_unwrap(nkb), (NkIrSymbolArray){NKS_INIT(*mod)});
    nk_llvm_optimizeIr(llvm_mod, tgt_unwrap(tgt), NkLlvmOptLevel_O3); // TODO: Hardcoded opt level

    TRY(nk_llvm_emitObjectFile(llvm_mod, tgt_unwrap(tgt), obj_file), false);

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

bool nkir_exportModule(NkbState nkb, NkIrModule mod, NkIrTarget tgt, NkString out_file, NkIrOutputKind kind) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod && tgt, false);

    bool ret = false;
    NkArena *scratch = nk_arena_getScratch(NULL);
    NK_ARENA_SCOPE(scratch) {
        ret = exportModuleImpl(scratch, nkb, mod, tgt, out_file, kind);
    }
    return ret;
}

NkIrRuntime nkir_createRuntime(NkArena *arena, NkbState nkb) {
    NK_LOG_TRC("%s", __func__);

    TRY(nkb, NULL);

    return rt_wrap(nk_llvm_createJitState(arena, nkb_unwrap(nkb)));
}

void nkir_freeRuntime(NkIrRuntime rt) {
    TRY(rt);

    nk_llvm_freeJitState(rt_unwrap(rt));
}

NkIrDylib nkir_createDylib(NkArena *arena, NkbState nkb, NkIrRuntime rt, NkIrModule mod) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod, NULL);

    NkIrDylib dl = nk_arena_allocT(arena, NkIrDylib_T);
    *dl = (NkIrDylib_T){
        .rt = rt,
        .mod = mod,
        .rt_loaded_syms = {.alloc = nk_arena_getAllocator(arena)},
        .llvm_jit_dylib = nk_llvm_createJitDylib(nkb_unwrap(nkb), rt_unwrap(rt)),
    };
    return dl;
}

void nkir_setSymbolResolver(NkIrDylib dl, NkIrSymbolResolver fn, void *userdata) {
    TRY(dl);

    nk_assert(!dl->sym_resolver_fn && "overwriting existing symbol resolver");

    dl->sym_resolver_fn = fn;
    dl->sym_resolver_userdata = userdata;
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

                        case NkIrArg_PhiArgsArray:
                            NK_ITERATE(NkIrPhiArg const *, phi_arg, arg->phi_args) {
                                if (phi_arg->ref.kind == NkIrRef_Global) {
                                    nkda_append(out, phi_arg->ref.sym);
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

static bool getSymbolDependencies(NkIrModule mod, NkAtom sym_name, NkIrSymbolDynArray *out) {
    NK_LOG_TRC("%s", __func__);

    bool ok = true;
    NK_PROF_FUNC() {
        NkArena *scratch = nk_arena_getScratch(nk_arena_getOptArenaFromAllocator(out->alloc));
        NK_ARENA_SCOPE(scratch) {
            NK_LOG_STREAM_DBG {
                NkStream log = nk_log_getStream();
                nk_print(log, "Getting dependencies for `");
                nkir_printSymbolName(log, sym_name);
                nk_print(log, "`");
            }

            NkAtomDynArray stack = {.alloc = nk_arena_getAllocator(scratch)};
            NkAtomSet deps = {.alloc = nk_arena_getAllocator(scratch)};

            nkda_append(&stack, sym_name);

            while (stack.size) {
                NkAtom const sym_name = NKS_LAST(stack);
                nkda_pop(&stack, 1);

                if (!NkAtomSet_find(&deps, sym_name)) {
                    NkAtomSet_insert(&deps, sym_name);

                    NkIrSymbol const *sym = NkIrSymbolDynArray_findItem(mod, sym_name);
                    if (!sym) {
                        NkStringBuilder sym_name_str = {.alloc = nk_arena_getAllocator(scratch)};
                        nkir_printSymbolName(nksb_getStream(&sym_name_str), sym_name);
                        nk_error_printf("symbol `" NKS_FMT "` not found, invalid ir", NKS_ARG(sym_name_str));
                        ok = false;
                        break;
                    }
                    gatherDeps(sym, &stack);
                }
            }

            if (ok) {
                NK_ITERATE(NkAtomSet_Item const *, it, deps) {
                    NkIrSymbol const *sym = NkIrSymbolDynArray_findItem(mod, it->key);
                    if (!sym) {
                        NkStringBuilder sym_name_str = {.alloc = nk_arena_getAllocator(scratch)};
                        nkir_printSymbolName(nksb_getStream(&sym_name_str), it->key);
                        nk_error_printf("symbol `" NKS_FMT "` not found, invalid ir", NKS_ARG(sym_name_str));
                        ok = false;
                        break;
                    }
                    nkda_append(out, *sym);
                }
            }
        }
    }

    return ok;
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

static void *getSymbolAddressImpl(NkArena *scratch, NkbState nkb, NkIrDylib dl, NkAtom sym_name) {
    NkIrModule mod = dl->mod;

    NkIrSymbolDynArray deps = {.alloc = nk_arena_getAllocator(scratch)};
    TRY(getSymbolDependencies(mod, sym_name, &deps), NULL);

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_print(log, "Dependencies: [ ");
        NK_ITERATE(NkIrSymbol *, sym, deps) {
            if (NK_INDEX(sym, deps)) {
                nk_print(log, ", ");
            }
            nkir_printSymbolName(log, sym->name);
        }
        nk_print(log, " ]");
    }

    NkIrSymbolAddressDynArray to_define = {.alloc = nk_arena_getAllocator(scratch)};

    NK_ITERATE(NkIrSymbol *, dep, deps) {
        if (NkAtomSet_find(&dl->rt_loaded_syms, dep->name)) {
            if (dep->kind == NkIrSymbol_Proc || dep->kind == NkIrSymbol_Data) {
                *dep = symToExtern(scratch, *dep);
            }
        } else {
            NkAtomSet_insert(&dl->rt_loaded_syms, dep->name);

            if (dep->kind == NkIrSymbol_Extern) {
                nk_assert(dl->sym_resolver_fn && "Symbol resolver is not set up");

                void *addr = dl->sym_resolver_fn(dep->name, dl->sym_resolver_userdata);
                if (!addr) {
                    NkStringBuilder sym_name_str = {.alloc = nk_arena_getAllocator(scratch)};
                    nkir_printSymbolName(nksb_getStream(&sym_name_str), sym_name);
                    NkStringBuilder dep_name_str = {.alloc = nk_arena_getAllocator(scratch)};
                    nkir_printSymbolName(nksb_getStream(&dep_name_str), dep->name);
                    nk_error_printf(
                        "Failed to get address of `" NKS_FMT "`, dependency `" NKS_FMT "` not found",
                        NKS_ARG(sym_name_str),
                        NKS_ARG(dep_name_str));
                    return NULL;
                }

                nkda_append(
                    &to_define,
                    ((NkIrSymbolAddress){
                        .sym = dep->name,
                        .addr = addr,
                    }));
            }
        }
    }

    NkLlvmJitState jit = rt_unwrap(dl->rt);
    NkLlvmJitDylib jdl = dl->llvm_jit_dylib;

    nk_llvm_defineExternSymbols(jit, jdl, (NkIrSymbolAddressArray){NKS_INIT(to_define)});

    NkLlvmModule llvm_mod = nk_llvm_compileIr(nkb_unwrap(nkb), (NkIrSymbolArray){NKS_INIT(deps)});

    NkLlvmTarget tgt = nk_llvm_getJitTarget(jit);
    nk_llvm_optimizeIr(llvm_mod, tgt, NkLlvmOptLevel_O3); // TODO: Hardcoded opt level

    nk_llvm_jitModule(llvm_mod, jit, jdl);

    return nk_llvm_getSymbolAddress(jit, jdl, sym_name);
}

void *nkir_getSymbolAddress(NkbState nkb, NkIrDylib dl, NkAtom sym) {
    NK_LOG_TRC("%s", __func__);

    TRY(dl, false);

    void *addr = NULL;
    NK_PROF_FUNC() {
        NkIrModule mod = dl->mod;

        if (NkIrSymbolDynArray_findItem(mod, sym)) {
            NkArena *scratch = nk_arena_getScratch(NULL);
            NK_ARENA_SCOPE(scratch) {
                addr = getSymbolAddressImpl(scratch, nkb, dl, sym);
            }
        } else {
            nk_error_printf("Symbol not found: %s", nk_atom2cs(sym));
        }
    }

    return addr;
}

bool nkir_defineExternSymbols(NkIrDylib dl, NkIrSymbolAddressArray syms) {
    NK_LOG_TRC("%s", __func__);

    TRY(dl, false);

    nk_llvm_defineExternSymbols(rt_unwrap(dl->rt), dl->llvm_jit_dylib, syms);

    return true;
}

bool nkir_invoke(NkIrDylib dl, NkAtom sym, void **args, void **ret) {
    NK_LOG_TRC("%s", __func__);

    TRY(dl, false);

    (void)dl;
    (void)sym;
    (void)args;
    (void)ret;
    nk_assert(!"TODO: `nkir_invoke` not implemented");
    return false;
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

void nkir_inspectModule(NkStream out, NkIrModule mod) {
    NK_ITERATE(NkIrSymbol const *, sym, *mod) {
        nk_print(out, "\n");
        nkir_inspectSymbol(out, sym);
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
    bool write_idx;
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
    } else if (ctx.write_idx) {
        nk_printf(out, "%5zu |%7s ", idx, s_opcode_names[instr->code]);
    } else {
        nk_printf(out, "%s ", s_opcode_names[instr->code]);
    }

    for (usize ai = 0; ai < 3; ai++) {
        usize const arg_idx = (ai + 1) % 3;

        NkIrArg const *arg = &instr->arg[arg_idx];

        if (arg->kind != NkIrArg_None) {
            if (arg_idx == 0) {
                nk_print(out, " -> ");
            } else if (arg_idx == 2) {
                nk_print(out, ", ");
            }
        }

        switch (arg->kind) {
            case NkIrArg_None:
                break;

            case NkIrArg_Ref:
                nkir_inspectRef(out, arg->ref);
                break;

            case NkIrArg_RefArray:
                nk_print(out, "(");
                NK_ITERATE(NkIrRef const *, ref, arg->refs) {
                    if (NK_INDEX(ref, arg->refs)) {
                        nk_print(out, ", ");
                    }
                    nkir_inspectRef(out, *ref);
                }
                nk_print(out, ")");
                break;

            case NkIrArg_PhiArgsArray:
                nk_print(out, "(");
                NK_ITERATE(NkIrPhiArg const *, phi_arg, arg->phi_args) {
                    if (NK_INDEX(phi_arg, arg->phi_args)) {
                        nk_print(out, ", ");
                    }
                    nk_printf(out, "@%s ", nk_atom2cs(phi_arg->label));
                    nkir_inspectRef(out, phi_arg->ref);
                }
                nk_print(out, ")");
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
                nk_print(out, ":");
                nkir_inspectType(out, arg->type);
                break;

            case NkIrArg_String:
                nk_printf(out, NKS_FMT, NKS_ARG(arg->str));
                break;
        }
    }
}

static void inspectVal(NkStream out, void *base_addr, usize base_offset, NkIrRelocArray relocs, NkIrType type) {
    if (!base_addr) {
        nk_print(out, "(null)");
        return;
    }
    switch (type->kind) {
        case NkIrType_Aggregate:
            nk_print(out, "{");
            NK_ITERATE(NkIrAggregateElemInfo const *, elem, type->aggr) {
                if (NK_INDEX(elem, type->aggr)) {
                    nk_print(out, ", ");
                }
                usize offset = base_offset + elem->offset;
                if (elem->type->kind == NkIrType_Numeric && elem->type->size == 1) {
                    char const *addr = (char *)base_addr + offset;
                    nk_print(out, "\"");
                    nks_escape(out, (NkString){addr, elem->count});
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
                        nk_print(out, "]");
                    }
                }
            }
            nk_print(out, "}");
            break;

        case NkIrType_Numeric:
        case NkIrType_Pointer: {
            void *addr = (u8 *)base_addr + base_offset;
            nkir_inspectVal(out, addr, type);
            break;
        }
    }
}

void nkir_inspectSymbol(NkStream out, NkIrSymbol const *sym) {
    switch (sym->vis) {
        case NkIrVisibility_Default:
            nk_print(out, "pub ");
            break;
        case NkIrVisibility_Local:
            nk_print(out, "local ");
            break;
        case NkIrVisibility_Unknown:
        case NkIrVisibility_Hidden: // TODO: Support other visibilities
        case NkIrVisibility_Protected:
        case NkIrVisibility_Internal:
            break;
    }

    if (sym->flags & NkIrSymbol_ThreadLocal) {
        nk_print(out, "thread_local ");
    }

    switch (sym->kind) {
        case NkIrSymbol_None:
            break;

        case NkIrSymbol_Proc: {
            NkArena *scratch = nk_arena_getScratch(nksb_getOptArenaFromStream(out));
            NK_ARENA_SCOPE(scratch) {
                LabelDynArray da_labels = {.alloc = nk_arena_getAllocator(scratch)};
                LabelArray const labels = collectLabels(sym->proc.instrs, &da_labels);

                u32 *indices = countLabels(scratch, labels);

                nk_print(out, "proc $");
                nkir_printSymbolName(out, sym->name);
                nk_print(out, "(");
                NK_ITERATE(NkIrParam const *, param, sym->proc.params) {
                    if (NK_INDEX(param, sym->proc.params)) {
                        nk_print(out, ", ");
                    }
                    nk_print(out, ":");
                    nkir_inspectType(out, param->type);
                    if (param->name) {
                        nk_printf(out, " %%%s", nk_atom2cs(param->name));
                    }
                }
                nk_print(out, ") :");
                nkir_inspectType(out, sym->proc.ret.type);
                if (sym->proc.ret.name) {
                    nk_printf(out, " %%%s", nk_atom2cs(sym->proc.ret.name));
                }
                nk_print(out, " {\n");
                NK_ITERATE(NkIrInstr const *, instr, sym->proc.instrs) {
                    inspectInstrImpl(
                        out,
                        NK_INDEX(instr, sym->proc.instrs),
                        (InspectInstrCtx){
                            .instrs = sym->proc.instrs,
                            .labels = labels,
                            .indices = indices,
                            .write_idx = true,
                        });
                    nk_print(out, "\n");
                }
                nk_print(out, "}");
            }
            break;
        }

        case NkIrSymbol_Data:
            if (sym->data.flags & NkIrData_ReadOnly) {
                nk_print(out, "const ");
            } else {
                nk_print(out, "data ");
            }
            // TODO: Inline strings
            nk_print(out, "$");
            nkir_printSymbolName(out, sym->name);
            nk_print(out, " :");
            nkir_inspectType(out, sym->data.type);
            if (sym->data.addr) {
                nk_print(out, " ");
                inspectVal(out, sym->data.addr, 0, sym->data.relocs, sym->data.type);
            }
            break;

        case NkIrSymbol_Extern:
            nk_print(out, "extern ");
            if (sym->extrn.lib) {
                nk_print(out, "\"");
                nks_escape(out, nk_atom2s(sym->extrn.lib));
                nk_print(out, "\" ");
            }
            switch (sym->extrn.kind) {
                case NkIrExtern_Proc:
                    nk_print(out, "proc $");
                    nkir_printSymbolName(out, sym->name);
                    nk_print(out, "(");
                    NK_ITERATE(NkIrType const *, type, sym->extrn.proc.param_types) {
                        if (NK_INDEX(type, sym->extrn.proc.param_types)) {
                            nk_print(out, ", ");
                        }
                        nk_print(out, ":");
                        nkir_inspectType(out, *type);
                    }
                    if (sym->extrn.proc.flags & NkIrProc_Variadic) {
                        nk_print(out, ", ...");
                    }
                    nk_print(out, ") :");
                    nkir_inspectType(out, sym->extrn.proc.ret_type);
                    break;

                case NkIrExtern_Data:
                    nk_print(out, "data $");
                    nkir_printSymbolName(out, sym->name);
                    nk_print(out, " :");
                    nkir_inspectType(out, sym->extrn.data.type);
                    break;
            }
            break;
    }

    nk_print(out, "\n");
}

void nkir_inspectInstr(NkStream out, NkIrInstr instr) {
    inspectInstrImpl(
        out,
        0,
        (InspectInstrCtx){
            .instrs = (NkIrInstrArray){&instr, 1},
            .write_idx = false,
        });
}

void nkir_inspectRef(NkStream out, NkIrRef ref) {
    if (ref.kind == NkIrRef_None) {
        return;
    }

    if (ref.kind == NkIrRef_VariadicMarker) {
        nk_print(out, "...");
        return;
    }

    nk_print(out, ":");
    nkir_inspectType(out, ref.type);

    switch (ref.kind) {
        case NkIrRef_Local:
        case NkIrRef_Param:
            nk_printf(out, " %%%s", nk_atom2cs(ref.sym));
            break;

        case NkIrRef_Global: {
            nk_print(out, " $");
            nkir_printSymbolName(out, ref.sym);
            break;
        }

        case NkIrRef_Imm:
            nk_print(out, " ");
            nkir_inspectVal(out, &ref.imm, ref.type);
            break;

        case NkIrRef_None:
        case NkIrRef_Null:
        case NkIrRef_VariadicMarker:
            break;
    }
}

bool nkir_validateModule(NkIrModule mod) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod, false);

    nk_assert(!"TODO: `nkir_validateModule` not implemented");
    return false;
}

bool nkir_validateProc(NkIrProc const *proc) {
    NK_LOG_TRC("%s", __func__);

    TRY(proc, false);

    nk_assert(!"TODO: `nkir_validateProc` not implemented");
    return false;
}
