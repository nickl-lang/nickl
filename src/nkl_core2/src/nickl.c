#include "nkl/core/nickl.h"

#include <assert.h>

#include "hash_trees.h"
#include "ir_parser.h"
#include "nickl_impl.h"
#include "nkb/ir.h"
#include "nkl/common/ast.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dl.h"
#include "ntk/dyn_array.h"
#include "ntk/error.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

NK_LOG_USE_SCOPE(nickl);

#define TRY(EXPR)      \
    do {               \
        if (!(EXPR)) { \
            return 0;  \
        }              \
    } while (0)

// TODO: Infer source location if operating during compilation

#define HANDLE_ERRORS()                                              \
    do {                                                             \
        NkErrorNode *_err = err.errors;                              \
        if (_err) {                                                  \
            while (_err) {                                           \
                nickl_reportError(nkl, NKS_FMT, NKS_ARG(_err->msg)); \
                _err = _err->next;                                   \
            }                                                        \
            return 0;                                                \
        }                                                            \
    } while (0)

NklState nkl_newState(void) {
    NK_LOG_TRC("%s", __func__);

    NkArena arena = {0};
    NklState nkl = nk_arena_allocT(&arena, NklState_T);
    *nkl = (NklState_T){
        .arena = arena,
        .nkb = nkir_createState(),
    };
    nkl->text_map = (NkAtomStringMap){.alloc = nk_arena_getAllocator(&nkl->arena)};
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

    NkErrorState err = {.alloc = nk_arena_getAllocator(&nkl->scratch)};

    NkIrTarget tgt = NULL;
    NK_ARENA_SCOPE(&nkl->scratch) {
        NkString const triple_str = targetTripleToString(&nkl->scratch, triple);

        NK_ERROR_SCOPE(&err) {
            tgt = nkir_createTarget(nkl->nkb, triple_str);
        }
    }

    HANDLE_ERRORS();

    NklCompiler com = nk_arena_allocT(&nkl->arena, NklCompiler_T);
    *com = (NklCompiler_T){
        .nkl = nkl,
        .lib_aliases = {.alloc = nk_arena_getAllocator(&nkl->arena)},
        .target = tgt,
    };
    return com;
}

NklCompiler nkl_newCompilerForHost(NklState nkl) {
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

static void *symbolResolver(NkAtom sym, void *userdata) {
    NK_LOG_TRC("%s", __func__);

    NklModule mod = userdata;

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_printf(log, "Searching for ");
        nickl_printSymbol(log, mod->name, sym);
    }

    NkAtom *found = NkAtomMap_find(&mod->extern_syms, sym);
    if (!found) {
        NK_LOG_DBG("  Not found");
        return NULL;
    }
    NkAtom const mod_name = *found;
    if (!mod_name) {
        NK_LOG_DBG("  Not found");
        return NULL;
    }

    NklModule *found_mod = NkAtomModuleMap_find(&mod->linked_mods, mod_name);
    if (found_mod) {
        NK_LOG_STREAM_DBG {
            NkStream log = nk_log_getStream();
            nk_printf(log, "  Found in ");
            nickl_printModuleName(log, mod_name);
        }

        // TODO: Detect cycles during symbol resolution
        NklModule const src_mod = *found_mod;
        return nkir_getSymbolAddress(src_mod->ir, sym);
    } else {
        NkAtom const lib = nickl_translateLib(mod->com, mod_name);

        NK_LOG_STREAM_DBG {
            NkStream log = nk_log_getStream();
            nk_printf(log, "  Found in ");
            nickl_printModuleName(log, mod_name);
            if (lib != mod_name) {
                nk_printf(log, " aka \"%s\"", nk_atom2cs(lib));
            }
        }

        NkHandle dl = nkdl_loadLibrary(nk_atom2cs(lib));
        if (nk_handleIsNull(dl)) {
            nickl_reportError(
                mod->com->nkl, "Failed to load library `%s`: %s", nk_atom2cs(lib), nkdl_getLastErrorString());
            return NULL;
        }

        void *addr = nkdl_resolveSymbol(dl, nk_atom2cs(sym));
        if (!addr) {
            nickl_reportError(
                mod->com->nkl, "Failed to load symbol `%s`: %s", nk_atom2cs(sym), nkdl_getLastErrorString());
            return NULL;
        }

        return addr;
    }
}

static NklModule newModuleImpl(NklCompiler com, NkAtom name) {
    TRY(com);

    NklState nkl = com->nkl;

    NklModule mod = nk_arena_allocT(&nkl->arena, NklModule_T);
    *mod = (NklModule_T){
        .name = name,

        .com = com,
        .ir = nkir_createModule(nkl->nkb),

        .linked_mods = {.alloc = nk_arena_getAllocator(&nkl->arena)},
        .extern_syms = {.alloc = nk_arena_getAllocator(&nkl->arena)},

        .mods_linked_to = {.alloc = nk_arena_getAllocator(&nkl->arena)},
    };

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_printf(log, "Creating module ");
        nickl_printModuleName(log, mod->name);
    }

    nkir_setSymbolResolver(mod->ir, symbolResolver, mod);

    return mod;
}

NklModule nkl_newModule(NklCompiler com) {
    NK_LOG_TRC("%s", __func__);

    return newModuleImpl(com, nk_atom_unique((NkString){0}));
}

NklModule nkl_newModuleNamed(NklCompiler com, NkString name) {
    NK_LOG_TRC("%s", __func__);

    return newModuleImpl(com, nk_s2atom(name));
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

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_printf(log, "Linking ");
        nickl_printModuleName(log, dst_mod->name);
        nk_printf(log, " <- ");
        nickl_printModuleName(log, src_mod->name);
    }

    NkAtomModuleMap_insert(&dst_mod->linked_mods, src_mod->name, src_mod);

    nkda_append(&src_mod->mods_linked_to, dst_mod);

    NK_ITERATE(NkIrSymbol const *, sym, nkir_moduleGetSymbols(src_mod->ir)) {
        if (!nickl_linkSymbol(dst_mod, src_mod, sym)) {
            return false;
        }
    }

    return true;
}

bool nkl_addLibraryAlias(NklCompiler com, NkString alias, NkString lib) {
    NK_LOG_TRC("%s", __func__);

    TRY(com);

    // TODO: Validate input

    NkAtomMap_insert(&com->lib_aliases, nk_s2atom(alias), nk_s2atom(lib));

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

static_assert((int)NklOutput_None == NkIrOutput_None, "");
static_assert((int)NklOutput_Binary == NkIrOutput_Binary, "");
static_assert((int)NklOutput_Static == NkIrOutput_Static, "");
static_assert((int)NklOutput_Shared == NkIrOutput_Shared, "");
static_assert((int)NklOutput_Archiv == NkIrOutput_Archiv, "");
static_assert((int)NklOutput_Object == NkIrOutput_Object, "");

bool nkl_exportModule(NklModule mod, NkString out_file, NklOutputKind kind) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NklState nkl = mod->com->nkl;

    NkErrorState err = {.alloc = nk_arena_getAllocator(&nkl->scratch)};
    NK_ERROR_SCOPE(&err) {
        nkir_exportModule(mod->ir, mod->com->target, out_file, (NkIrOutputKind)kind);
    }
    HANDLE_ERRORS();

    return true;
}

void *nkl_getSymbolAddress(NklModule mod, NkString name) {
    NK_LOG_TRC("%s", __func__);

    TRY(mod);

    NkAtom const sym = nk_s2atom(name);

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_printf(log, "Resolving address of ");
        nickl_printSymbol(log, mod->name, sym);
    }

    NklState nkl = mod->com->nkl;

    void *addr = NULL;

    NkErrorState err = {.alloc = nk_arena_getAllocator(&nkl->scratch)};
    NK_ERROR_SCOPE(&err) {
        addr = nkir_getSymbolAddress(mod->ir, sym);
    }
    HANDLE_ERRORS();

    return addr;
}

NklError const *nkl_getErrors(NklState nkl) {
    return nkl->error;
}
