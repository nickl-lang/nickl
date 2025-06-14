#include "nkl/core/nickl.h"

#include <assert.h>

#include "ir_parser.h"
#include "nickl_impl.h"
#include "nkb/ir.h"
#include "nkl/common/ast.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/dyn_array.h"
#include "ntk/error.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(nickl);

NklState nkl_newState(void) {
    NK_LOG_TRC("%s", __func__);

    NkArena arena = {0};
    NklState nkl = nk_arena_allocT(&arena, NklState_T);
    *nkl = (NklState_T){
        .arena = arena,
        .nkb = nkir_createState(),
    };
    nkl->text_map = (NkIntptrHashTree){.alloc = nk_arena_getAllocator(&nkl->arena)};
    return nkl;
}

void nkl_freeState(NklState nkl) {
    NK_LOG_TRC("%s", __func__);

    nk_assert(nkl && "state is null");

    nk_arena_free(&nkl->scratch);

    nkir_freeState(nkl->nkb);

    NkArena arena = nkl->arena;
    nk_arena_free(&arena);
}

static NkString targetTripleToString(NkArena *arena, NklTargetTriple triple) {
    NkStringBuilder sb = {.alloc = nk_arena_getAllocator(arena)};
    nksb_printf(&sb, "%s-%s-%s", nk_atom2cs(triple.arch), nk_atom2cs(triple.vendor), nk_atom2cs(triple.sys));
    if (triple.abi) {
        nksb_printf(&sb, "-%s", nk_atom2cs(triple.abi));
    }
    return (NkString){NKS_INIT(sb)};
}

NklCompiler nkl_newCompiler(NklState nkl, NklTargetTriple triple) {
    NK_LOG_TRC("%s", __func__);

    NkIrTarget tgt = NULL;
    NK_ARENA_SCOPE(&nkl->scratch) {
        NkString const triple_str = targetTripleToString(&nkl->scratch, triple);
        tgt = nkir_createTarget(nkl->nkb, triple_str);
        if (!tgt) {
            nickl_reportError(nkl, "failed to create compiler for target `" NKS_FMT "`", NKS_ARG(triple_str));
            return NULL;
        }
    }

    NklCompiler com = nk_arena_allocT(&nkl->arena, NklCompiler_T);
    *com = (NklCompiler_T){
        .nkl = nkl,
        .lib_aliases = {.alloc = nk_arena_getAllocator(&nkl->arena)},
        .target = tgt,
    };
    return com;
}

NklCompiler nkl_newCompilerHost(NklState nkl) {
    NK_LOG_TRC("%s", __func__);

    return nkl_newCompiler(
        nkl,
        (NklTargetTriple){
            .arch = nk_cs2atom(HOST_TARGET_ARCH),
            .vendor = nk_cs2atom(HOST_TARGET_VENDOR),
            .sys = nk_cs2atom(HOST_TARGET_SYS),
            .abi = strlen(HOST_TARGET_ABI) ? nk_cs2atom(HOST_TARGET_ABI) : 0,
        });
}

#define TRY(EXPR)      \
    do {               \
        if (!(EXPR)) { \
            return 0;  \
        }              \
    } while (0)

NklModule nkl_newModule(NklCompiler com) {
    NK_LOG_TRC("%s", __func__);

    TRY(com);

    NklState nkl = com->nkl;

    NklModule mod = nk_arena_allocT(&nkl->arena, NklModule_T);
    *mod = (NklModule_T){
        .com = com,
        .ir = nkir_createModule(nkl->nkb),
        .linked_in = {.alloc = nk_arena_getAllocator(&nkl->arena)},
        .linked_to = {.alloc = nk_arena_getAllocator(&nkl->arena)},
    };
    return mod;
}

bool nkl_linkModule(NklModule dst_mod, NklModule src_mod) {
    NK_LOG_TRC("%s", __func__);

    TRY(dst_mod);

    NklState nkl = dst_mod->com->nkl;

    if (!src_mod) {
        nickl_reportError(nkl, "src_mod is null");
        return false;
    }

    if (dst_mod->com != src_mod->com) {
        nickl_reportError(nkl, "mixed modules from different compilers");
    }

    nkda_append(&dst_mod->linked_in, src_mod);
    nkda_append(&src_mod->linked_to, dst_mod);

    // TODO: Check for symbol conflicts

    return true;
}

bool nkl_addLibraryAlias(NklCompiler com, NkString alias, NkString lib) {
    NK_LOG_TRC("%s", __func__);

    TRY(com);

    NklState nkl = com->nkl;

    // TODO: Validate input

    nkda_append(
        &com->lib_aliases,
        ((LibAlias){
            .lib = nk_tsprintf(&nkl->arena, NKS_FMT, NKS_ARG(lib)),
            .alias = nk_tsprintf(&nkl->arena, NKS_FMT, NKS_ARG(alias)),
        }));

    return true;
}

bool nkl_compileFile(NklModule mod, NkString path) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->com->nkl;

    NkString const ext = nk_path_getExtension(path);

    if (nks_equal(ext, nk_cs2s("nkir"))) {
        return nkl_compileFileIr(mod, path);
    } else if (nks_equal(ext, nk_cs2s("nkst"))) {
        return nkl_compileFileAst(mod, path);
    } else if (nks_equal(ext, nk_cs2s("nkl"))) {
        return nkl_compileFileNkl(mod, path);
    } else {
        nickl_reportError(
            nkl, "Unsupported source file `*." NKS_FMT "`. Supported: `*.nkir`, `*.nkst`, `*.nkl`.", NKS_ARG(ext));
        return false;
    }
}

static bool compileIrImpl(NklModule mod, NkAtom file) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    TRY(nkl_ir_parse(&(NklIrParserData){
        .mod = mod,
        .file = file,
        .token_names = s_ir_tokens,
    }));

    return true;
}

static bool compileAstImpl(NklModule mod, NkAtom file) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->com->nkl;

    NklAstNodeArray nodes;
    TRY(nickl_getAst(nkl, file, &nodes));

    nickl_reportError(nkl, "TODO: `compileAstImpl` is not finished");
    return false;
}

static bool compileNklImpl(NklModule mod, NkAtom file) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->com->nkl;

    (void)file;
    nickl_reportError(nkl, "TODO: `compileNklImpl` is not implemented");
    return false;
}

bool nkl_compileFileIr(NklModule mod, NkString path) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->com->nkl;

    char cwd[NK_MAX_PATH];
    if (nk_getCwd(cwd, sizeof(cwd)) < 0) {
        nickl_reportError(nkl, NKS_FMT ": %s", NKS_ARG(path), nk_getLastErrorString());
        return false;
    }

    NkAtom const file = nickl_canonicalizePath(nk_cs2s(cwd), path);
    if (!file) {
        nickl_reportError(nkl, NKS_FMT ": %s", NKS_ARG(path), nk_getLastErrorString());
        return false;
    }

    return compileIrImpl(mod, file);
}

bool nkl_compileFileAst(NklModule mod, NkString path) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->com->nkl;

    char cwd[NK_MAX_PATH];
    if (nk_getCwd(cwd, sizeof(cwd)) < 0) {
        nickl_reportError(nkl, NKS_FMT ": %s", NKS_ARG(path), nk_getLastErrorString());
        return false;
    }

    NkAtom const file = nickl_canonicalizePath(nk_cs2s(cwd), path);
    if (!file) {
        nickl_reportError(nkl, NKS_FMT ": %s", NKS_ARG(path), nk_getLastErrorString());
        return false;
    }

    return compileAstImpl(mod, file);
}

bool nkl_compileFileNkl(NklModule mod, NkString path) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->com->nkl;

    char cwd[NK_MAX_PATH];
    if (nk_getCwd(cwd, sizeof(cwd)) < 0) {
        nickl_reportError(nkl, NKS_FMT ": %s", NKS_ARG(path), nk_getLastErrorString());
        return false;
    }

    NkAtom const file = nickl_canonicalizePath(nk_cs2s(cwd), path);
    if (!file) {
        nickl_reportError(nkl, NKS_FMT ": %s", NKS_ARG(path), nk_getLastErrorString());
        return false;
    }

    return compileNklImpl(mod, file);
}

bool nkl_compileStringIr(NklModule mod, NkString src) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->com->nkl;

    NkAtom file = nk_atom_unique((NkString){0});
    TRY(nickl_defineText(nkl, file, src));

    return compileIrImpl(mod, file);
}

bool nkl_compileStringAst(NklModule mod, NkString src) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->com->nkl;

    NkAtom file = nk_atom_unique((NkString){0});
    TRY(nickl_defineText(nkl, file, src));

    return compileAstImpl(mod, file);
}

bool nkl_compileStringNkl(NklModule mod, NkString src) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->com->nkl;

    NkAtom file = nk_atom_unique((NkString){0});
    TRY(nickl_defineText(nkl, file, src));

    return compileNklImpl(mod, file);
}

bool nkl_exportModule(NklModule mod, NkString out_file, NklOutputKind kind) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    // TODO: Handle errors

    static_assert((int)NklOutput_None == NkIrOutput_None, "");
    static_assert((int)NklOutput_Binary == NkIrOutput_Binary, "");
    static_assert((int)NklOutput_Static == NkIrOutput_Static, "");
    static_assert((int)NklOutput_Shared == NkIrOutput_Shared, "");
    static_assert((int)NklOutput_Archiv == NkIrOutput_Archiv, "");
    static_assert((int)NklOutput_Object == NkIrOutput_Object, "");

    nkir_exportModule(mod->ir, mod->com->target, out_file, (NkIrOutputKind)kind);

    return true;
}

void *nkl_getSymbolAddress(NklModule mod, NkString name) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    return nkir_getSymbolAddress(mod->ir, nk_s2atom(name));
}

NklError const *nkl_getErrors(NklState nkl) {
    return nkl->error;
}
