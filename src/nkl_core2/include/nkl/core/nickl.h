#ifndef NKL_CORE_NICKL_H_
#define NKL_CORE_NICKL_H_

#include "nkl/common/token.h"
#include "ntk/atom.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

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

    NklOutput_Object,
    NklOutput_Static,
    NklOutput_Shared,
    NklOutput_Binary,
} NklOutputKind;

typedef struct NklError {
    struct NklError *next;

    NkString msg;
    NkAtom file;
    NklToken const *token;
} NklError;

NK_EXPORT NklState nkl_newState();
NK_EXPORT void nkl_freeState(NklState nkl);

NK_EXPORT void nkl_pushState(NklState nkl);
NK_EXPORT void nkl_popState();

NK_EXPORT NklCompiler nkl_newCompiler(NklTargetTriple target);
NK_EXPORT NklCompiler nkl_newCompilerHost();

NK_EXPORT NklModule nkl_newModule(NklCompiler c);

NK_EXPORT bool nkl_linkModule(NklModule dst_mod, NklModule src_mod);

NK_EXPORT bool nkl_compileFile(NklModule mod, NkString in_file);

// *.nkir
NK_EXPORT bool nkl_compileFileIr(NklModule mod, NkString in_file);
// *.nkst
NK_EXPORT bool nkl_compileFileAst(NklModule mod, NkString in_file);
// *.nkl
NK_EXPORT bool nkl_compileFileNkl(NklModule mod, NkString in_file);

NK_EXPORT bool nkl_exportModule(NklModule mod, NkString out_file, NklOutputKind kind);

NK_EXPORT NklError const *nkl_getErrors(NklState nkl);

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NICKL_H_
