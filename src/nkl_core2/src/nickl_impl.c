#include "nickl_impl.h"

#include "ast_parser.h"
#include "ast_tokens.h"
#include "ir_tokens.h"
#include "nkb/ir.h"
#include "nkl/core/lexer.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/error.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/stream.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

NK_LOG_USE_SCOPE(nickl);

void nickl_reportError(NklState nkl, char const *fmt, ...) {
    NK_LOG_TRC("%s", __func__);

    va_list ap;
    va_start(ap, fmt);
    nickl_vreportError(nkl, (NklSourceLocation){0}, fmt, ap);
    va_end(ap);
}

void nickl_reportErrorLoc(NklState nkl, NklSourceLocation loc, char const *fmt, ...) {
    NK_LOG_TRC("%s", __func__);

    va_list ap;
    va_start(ap, fmt);
    nickl_vreportError(nkl, loc, fmt, ap);
    va_end(ap);
}

void nickl_vreportError(NklState nkl, NklSourceLocation loc, char const *fmt, va_list ap) {
#ifdef ENABLE_LOGGING
    {
        va_list ap_copy;
        va_copy(ap_copy, ap);
        NK_LOGV(NkLogLevel_Error, fmt, ap_copy);
        va_end(ap_copy);
    }
#endif // ENABLE_LOGGING

    NklError *err = nk_arena_allocT(&nkl->arena, NklError);
    *err = (NklError){
        .next = nkl->error,
        .msg = nk_vtsprintf(&nkl->arena, fmt, ap),
        .loc = loc,
    };
    nkl->error = err;
}

#define TRY(EXPR)      \
    do {               \
        if (!(EXPR)) { \
            return 0;  \
        }              \
    } while (0)

static void defineTextImpl(NklState nkl, NkAtom file, NkString text) {
    NkAtomStringMap_insert(&nkl->text_map, file, text);
}

void nickl_printModuleName(NkStream out, NklModule mod) {
    nkir_printName(out, "module", mod->name);
}

void nickl_printSymbol(NkStream out, NklModule mod, NkAtom sym) {
    nickl_printModuleName(out, mod);
    nk_printf(out, "::");
    nkir_printSymbolName(out, sym);
}

bool nickl_getText(NklState nkl, NkAtom file, NkString *out_text) {
    NK_LOG_TRC("%s", __func__);

    NkString text;

    NkString *found = NkAtomStringMap_find(&nkl->text_map, file);
    if (found) {
        NK_LOG_STREAM_DBG {
            NkStream log = nk_log_getStream();
            nk_printf(log, "Using cached text for file \"");
            nkir_printName(log, "file", file);
            nk_printf(log, "\"");
        }
        text = *found;
    } else {
        NK_LOG_DBG("Loading text for file `%s`", nk_atom2cs(file));

        NkAllocator const alloc = nk_arena_getAllocator(&nkl->arena);

        if (!nk_file_read(alloc, nk_atom2s(file), &text)) {
            nickl_reportErrorLoc(nkl, (NklSourceLocation){0}, "%s: %s", nk_atom2cs(file), nk_getLastErrorString());
            return false;
        }

        defineTextImpl(nkl, file, text);
    }

    *out_text = text;
    return true;
}

bool nickl_defineText(NklState nkl, NkAtom file, NkString text) {
    NK_LOG_TRC("%s", __func__);

    defineTextImpl(nkl, file, nk_tsprintf(&nkl->arena, NKS_FMT, NKS_ARG(text)));
    return true;
}

char const *s_ir_tokens[] = {
    "end of file", // NklToken_Eof,

    "identifier",           // NklToken_Id,
    "integer constant",     // NklToken_Int,
    "hex integer constant", // NklToken_IntHex,
    "float constant",       // NklToken_Float,
    "string constant",      // NklToken_String,
    "string constant",      // NklToken_EscapedString,
    "newline",              // NklToken_Newline,

    "error", // NklToken_Error,

    NULL, // NklIrToken_KeywordsBase,

    "cmp",     // NklIrToken_cmp,
    "const",   // NklIrToken_const,
    "data",    // NklIrToken_data,
    "extern",  // NklIrToken_extern,
    "include", // NklIrToken_include,
    "local",   // NklIrToken_local,
    "proc",    // NklIrToken_proc,
    "pub",     // NklIrToken_pub,
    "type",    // NklIrToken_type,
    "void",    // NklIrToken_void,

#define X(TYPE, VALUE_TYPE) #TYPE,
    NKIR_NUMERIC_ITERATE(X)
#undef X

#define IR(NAME) #NAME,
#define UNA_IR(NAME) #NAME,
#define BIN_IR(NAME) #NAME,
#define CMP_IR(NAME) #NAME,
#include "nkb/ir.inl"

        NULL, // NklIrToken_OperatorsBase,

    "(",   // NklIrToken_LParen,
    ")",   // NklIrToken_RParen,
    ",",   // NklIrToken_Comma,
    "->",  // NklIrToken_MinusGreater,
    "...", // NklIrToken_Ellipsis,
    ":",   // NklIrToken_Colon,
    "@+",  // NklIrToken_AtPlus,
    "@-",  // NklIrToken_AtMinus,
    "[",   // NklIrToken_LBracket,
    "]",   // NklIrToken_RBracket,
    "{",   // NklIrToken_LBrace,
    "|",   // NklIrToken_Pipe,
    "}",   // NklIrToken_RBrace,

    NULL, // NklIrToken_TagsBase,

    "$", // NklIrToken_DollarTag,
    "%", // NklIrToken_PercentTag,
    "@", // NklIrToken_AtTag,

    NULL,
};

bool nickl_getTokensIr(NklState nkl, NkAtom file, NklTokenArray *out_tokens) {
    NK_LOG_TRC("%s", __func__);

    // TODO: Cache tokens

    NkString text;
    TRY(nickl_getText(nkl, file, &text));

    NkString err_str = {0};
    if (!nkl_lex(
            &(NklLexerData){
                .text = text,
                .arena = &nkl->arena,
                .err_str = &err_str,

                .tokens = s_ir_tokens,
                .keywords_base = NklIrToken_KeywordsBase,
                .operators_base = NklIrToken_OperatorsBase,
                .tags_base = NklIrToken_TagsBase,
            },
            out_tokens)) {
        nk_assert(out_tokens->size);
        NklToken const err_token = nks_last(*out_tokens);
        nickl_reportErrorLoc(
            nkl,
            (NklSourceLocation){
                .file = nk_atom2s(file),
                .lin = err_token.lin,
                .col = err_token.col,
                .len = err_token.len,
            },
            NKS_FMT,
            NKS_ARG(err_str));
        return false;
    }

    return true;
}

char const *s_ast_tokens[] = {
    "end of file", // NklToken_Eof

    "identifier",           // NklToken_Id
    "integer constant",     // NklToken_Int
    "hex integer constant", // NklToken_IntHex,
    "float constant",       // NklToken_Float
    "string constant",      // NklToken_String
    "string constant",      // NklToken_EscapedString
    "newline",              // NklToken_Newline

    "error", // NklToken_Error

    NULL, // NklAstToken_OperatorsBase

    "(", // NklAstToken_LParen
    ")", // NklAstToken_RParen
    "{", // NklAstToken_LBrace
    "}", // NklAstToken_RBrace
    "[", // NklAstToken_LBraket
    "]", // NklAstToken_RBraket

    NULL,
};

bool nickl_getTokensAst(NklState nkl, NkAtom file, NklTokenArray *out_tokens) {
    NK_LOG_TRC("%s", __func__);

    // TODO: Cache tokens

    NkString text;
    TRY(nickl_getText(nkl, file, &text));

    NkString err_str = {0};
    if (!nkl_lex(
            &(NklLexerData){
                .text = text,
                .arena = &nkl->arena,
                .err_str = &err_str,

                .tokens = s_ast_tokens,
                .operators_base = NklAstToken_OperatorsBase,
            },
            out_tokens)) {
        nk_assert(out_tokens->size);
        NklToken const err_token = nks_last(*out_tokens);
        nickl_reportErrorLoc(
            nkl,
            (NklSourceLocation){
                .file = nk_atom2s(file),
                .lin = err_token.lin,
                .col = err_token.col,
                .len = err_token.len,
            },
            NKS_FMT,
            NKS_ARG(err_str));
        return false;
    }

    return true;
}

bool nickl_getAst(NklState nkl, NkAtom file, NklAstNodeArray *out_nodes) {
    NK_LOG_TRC("%s", __func__);

    // TODO: Cache ast

    TRY(nkl_ast_parse(
        &(NklAstParserData){
            .nkl = nkl,
            .file = file,
            .token_names = s_ast_tokens,
        },
        out_nodes));

    return true;
}

NkAtom nickl_canonicalizePath(NkString base, NkString path) {
    // TODO: Do null termination in ntk?

    NKSB_FIXED_BUFFER(sb, NK_MAX_PATH);

    nksb_printf(&sb, NKS_FMT, NKS_ARG(path));
    nksb_appendNull(&sb);

    char const *path_nt = sb.data;

    if (nk_pathIsRelative(path_nt)) {
        nksb_clear(&sb);

        nksb_printf(&sb, NKS_FMT "%c" NKS_FMT, NKS_ARG(base), NK_PATH_SEPARATOR, NKS_ARG(path));
        nksb_appendNull(&sb);
    }

    char path_canon[NK_MAX_PATH];
    if (nk_fullPath(path_canon, path_nt) < 0) {
        return 0;
    }

    return nk_cs2atom(path_canon);
}

NkAtom nickl_findFile(NklState nkl, NkAtom base, NkString name) {
    NK_LOG_TRC("%s", __func__);

    // TODO: Add include search paths
    (void)nkl;

    NkString const base_dir = nk_path_getParent(nk_atom2s(base));
    return nickl_canonicalizePath(base_dir, name);
}

NkAtom nickl_translateLib(NklCompiler com, NkAtom alias) {
    NkAtom *found = NkAtomMap_find(&com->lib_aliases, alias);
    return found ? *found : alias;
}

static char const *getSymbolKind(NkIrSymbol const *sym) {
    switch (sym->kind) {
        case NkIrSymbol_None:
            break;
        case NkIrSymbol_Proc:
            return "proc";
        case NkIrSymbol_Data:
            return "data";
        case NkIrSymbol_Extern:
            switch (sym->extrn.kind) {
                case NkIrExtern_Proc:
                    return "proc";
                case NkIrExtern_Data:
                    return "data";
            }
            break;
    }
    return "";
}

static bool defineSymbol(NklModule mod, NkIrSymbol const *sym, NkAtom NK_UNUSED lib) {
    NK_LOG_TRC("%s", __func__);

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_printf(log, "Defining ");
        if (sym->kind == NkIrSymbol_Extern) {
            nk_printf(log, "extern \"%s\" ", lib ? nk_atom2cs(lib) : "");
        }
        nk_printf(log, "%s ", getSymbolKind(sym));
        nickl_printSymbol(log, mod, sym->name);
    }

    NK_LOG_STREAM_INF {
        NkArena *scratch = &mod->com->nkl->scratch;
        NK_ARENA_SCOPE(scratch) {
            NkStream log = nk_log_getStream();
            nk_printf(log, "symbol:\n");
            nkir_inspectSymbol(log, scratch, sym);
        }
    }

    // TODO: Check for symbol conflicts
    nkir_moduleDefineSymbol(mod->ir, sym);
    return true;
}

static bool defineIntern(NklModule mod, NkIrSymbol const *sym) {
    if (defineSymbol(mod, sym, 0)) {
        if (sym->vis == NkIrVisibility_Default) {
            NK_ITERATE(NklModule const *, it, mod->mods_linked_to) {
                NklModule const dst_mod = *it;
                nickl_linkSymbol(dst_mod, mod, sym);
            }
        }

        return true;
    }
    return false;
}

static bool defineExtern(NklModule mod, NkIrSymbol const *sym, NkAtom lib) {
    if (defineSymbol(mod, sym, lib)) {
        NkAtomMap_insert(&mod->extern_syms, sym->name, lib);
        return true;
    }
    return false;
}

bool nickl_defineProc(NklModule mod, NkIrProc const *proc, NklSymbolInfo const *info) {
    return defineIntern(
        mod,
        &(NkIrSymbol){
            .proc = *proc,
            .name = info->name,
            .vis = info->vis,
            .flags = info->flags,
            .kind = NkIrSymbol_Proc,
        });
}

bool nickl_defineData(NklModule mod, NkIrData const *data, NklSymbolInfo const *info) {
    return defineIntern(
        mod,
        &(NkIrSymbol){
            .data = *data,
            .name = info->name,
            .vis = info->vis,
            .flags = info->flags,
            .kind = NkIrSymbol_Data,
        });
}

bool nickl_defineExtern(NklModule mod, NkIrExtern const *extrn, NklSymbolInfo const *info, NkAtom lib) {
    return defineExtern(
        mod,
        &(NkIrSymbol){
            .extrn = *extrn,
            .name = info->name,
            .vis = info->vis,
            .flags = info->flags,
            .kind = NkIrSymbol_Extern,
        },
        lib);
}

bool nickl_linkSymbol(NklModule dst_mod, NklModule src_mod, NkIrSymbol const *sym) {
    NK_LOG_TRC("%s", __func__);

    NklState nkl = dst_mod->com->nkl;

    if (sym->vis != NkIrVisibility_Default) {
        return true;
    }

    NK_LOG_STREAM_DBG {
        NkStream log = nk_log_getStream();
        nk_printf(log, "Linking ");
        nickl_printSymbol(log, dst_mod, sym->name);
        nk_printf(log, " <- ");
        nickl_printSymbol(log, src_mod, sym->name);
    }

    {
        NkAtom *found = NkAtomMap_find(&dst_mod->extern_syms, sym->name);
        if (found) {
            NkAtom const found_name = *found;
            if (found_name) {
                if (found_name != src_mod->name) {
                    NK_LOG_ERR("Symbol already exists with lib `%s`", nk_atom2cs(found_name));
                    nickl_reportError(nkl, "TODO: Symbol conflict");
                    return false;
                }
            } else {
                *found = src_mod->name;
            }
        } else {
            NkAtomMap_insert(&dst_mod->extern_syms, sym->name, src_mod->name);
        }
    }

    {
        NkIrSymbol const *found = nkir_findSymbol(dst_mod->ir, sym->name);
        if (found && found->kind != NkIrSymbol_Extern) {
            NK_LOG_ERR("TODO: Symbol conflict");
            nickl_reportError(nkl, "TODO: Symbol conflict");
            return false;
        }
    }

    return true;
}
