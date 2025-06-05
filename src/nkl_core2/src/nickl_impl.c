#include "nickl_impl.h"

#include "ast_parser.h"
#include "ast_tokens.h"
#include "ir_tokens.h"
#include "nkl/core/lexer.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/error.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/path.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

_Thread_local NklState s_nkl;

NK_LOG_USE_SCOPE(nickl);

NK_PRINTF_LIKE(3) void nickl_reportError(NklState nkl, NklSourceLocation loc, char const *fmt, ...) {
    NK_LOG_TRC("%s", __func__);

    va_list ap;
    va_start(ap, fmt);
    nickl_vreportError(nkl, loc, fmt, ap);
    va_end(ap);
}

void nickl_vreportError(NklState nkl, NklSourceLocation loc, char const *fmt, va_list ap) {
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

bool nickl_getText(NklState nkl, NkAtom file, NkString *out_text) {
    NK_LOG_TRC("%s", __func__);

    // TODO: Cache text

    NkAllocator const alloc = nk_arena_getAllocator(&nkl->arena);

    if (!nk_file_read(alloc, nk_atom2s(file), out_text)) {
        nickl_reportError(nkl, (NklSourceLocation){0}, "%s: %s", nk_atom2cs(file), nk_getLastErrorString());
        return false;
    }

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
        NklToken const err_token = nk_slice_last(*out_tokens);
        nickl_reportError(
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
        NklToken const err_token = nk_slice_last(*out_tokens);
        nickl_reportError(
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

NkString nickl_translateLib(NklModule mod, NkString alias) {
    NK_ITERATE(LibAlias const *, it, mod->lib_aliases) {
        if (nks_equal(alias, it->alias)) {
            return it->lib;
        }
    }

    NK_ITERATE(LibAlias const *, it, mod->com->lib_aliases) {
        if (nks_equal(alias, it->alias)) {
            return it->lib;
        }
    }

    return alias;
}
