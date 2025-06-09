#ifndef NKL_CORE_NICKL_H_
#define NKL_CORE_NICKL_H_

#include "nkl/common/diagnostics.h"
#include "ntk/atom.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Provide a C version for Nickl API without dependencies

typedef struct NklState_T *NklState;
typedef struct NklCompiler_T *NklCompiler;
typedef struct NklModule_T *NklModule;

typedef struct {
    NkAtom arch;
    NkAtom vendor;
    NkAtom sys;
} NklTargetTriple;

typedef enum {
    NklOutput_None = 0,

    NklOutput_Binary,
    NklOutput_Static,
    NklOutput_Shared,
    NklOutput_Archiv,
    NklOutput_Object,
} NklOutputKind;

typedef struct NklError {
    struct NklError *next;

    NkString msg;
    NklSourceLocation loc;
} NklError;

NK_EXPORT NklState nkl_newState(void);
NK_EXPORT void nkl_freeState(NklState nkl);

NK_EXPORT NklCompiler nkl_newCompiler(NklState nkl, NklTargetTriple target);
NK_EXPORT NklCompiler nkl_newCompilerHost(NklState nkl);

NK_EXPORT NklModule nkl_newModule(NklCompiler com);

NK_EXPORT bool nkl_linkModule(NklModule dst_mod, NklModule src_mod);

NK_EXPORT bool nkl_addLibraryAlias(NklCompiler com, NkString alias, NkString lib);

NK_EXPORT bool nkl_compileFile(NklModule mod, NkString path);
NK_EXPORT bool nkl_compileFileIr(NklModule mod, NkString path);  // *.nkir
NK_EXPORT bool nkl_compileFileAst(NklModule mod, NkString path); // *.nkst
NK_EXPORT bool nkl_compileFileNkl(NklModule mod, NkString path); // *.nkl

NK_EXPORT bool nkl_exportModule(NklModule mod, NkString out_file, NklOutputKind kind);

NK_EXPORT void *nkl_getSymbolAddress(NklModule mod, NkString name);

// TODO: Remove runModule in favor of getSymbolAddress
NK_EXPORT bool nkl_runModule(NklModule mod);

NK_EXPORT NklError const *nkl_getErrors(NklState nkl);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_H_
