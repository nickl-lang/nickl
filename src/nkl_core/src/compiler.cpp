#include "nkl/core/compiler.h"

#include "compiler_state.hpp"
#include "nkb/common.h"
#include "nkb/ir.h"
#include "nkl/common/ast.h"
#include "nkl/common/token.h"
#include "nkl/core/nickl.h"
#include "nkl/core/types.h"
#include "nodes.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/dyn_array.h"
#include "ntk/list.h"
#include "ntk/log.h"
#include "ntk/os/path.h"
#include "ntk/path.h"
#include "ntk/profiler.h"
#include "ntk/slice.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/utils.h"

namespace {

NK_LOG_USE_SCOPE(compiler);

struct Void {};

#define CHECK(EXPR)            \
    EXPR;                      \
    if (nkl_getErrorCount()) { \
        return {};             \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))
#define ASSIGN(SLOT, VAL) CHECK(SLOT = (VAL))
#define APPEND(AR, VAL) CHECK(nkda_append((AR), (VAL)))

#ifndef SCNi8
#define SCNi8 "hhi"
#endif // SCNi8

#ifndef SCNu8
#define SCNu8 "hhu"
#endif // SCNu8

#ifndef SCNx8
#define SCNx8 "hhx"
#endif // SCNx8

#define SCNf32 "f"
#define SCNf64 "lf"

#define SCNXi8 SCNx8
#define SCNXu8 SCNx8
#define SCNXi16 SCNx16
#define SCNXu16 SCNx16
#define SCNXi32 SCNx32
#define SCNXu32 SCNx32
#define SCNXi64 SCNx64
#define SCNXu64 SCNx64

#define ENTRY_POINT_NAME "main"

} // namespace

NklCompiler nkl_createCompiler(NklState nkl, NklTargetTriple target) {
    NkArena arena{};
    auto c = new (nk_arena_allocT<NklCompiler_T>(&arena)) NklCompiler_T{
        .ir{},
        .top_level_proc{NKIR_INVALID_IDX},
        .entry_point{NKIR_INVALID_IDX},

        .run_ctx_temp_arena{},
        .run_ctx{},

        .nkl = nkl,
        .perm_arena = arena,
        .temp_arenas{},

        .files{},
        .errors{},

        .word_size = 8, // TODO: Hardcoded word size, need to map it from target
    };
    c->files.alloc = nk_arena_getAllocator(&c->perm_arena);
    c->errors.arena = &c->perm_arena;
    c->ir = nkir_createProgram(&c->perm_arena);
    c->run_ctx = nkir_createRunCtx(c->ir, &c->run_ctx_temp_arena);
    return c;
}

void nkl_freeCompiler(NklCompiler c) {
    nkir_freeRunCtx(c->run_ctx);

    nk_arena_free(&c->temp_arenas[0]);
    nk_arena_free(&c->temp_arenas[1]);

    auto perm_arena = c->perm_arena;
    nk_arena_free(&perm_arena);
}

usize nkl_getCompileErrorCount(NklCompiler c) {
    return c->errors.error_count;
}

NklError *nkl_getCompileErrorList(NklCompiler c) {
    return c->errors.errors;
}

NklModule nkl_createModule(NklCompiler c) {
    auto const alloc = nk_arena_getAllocator(&c->perm_arena);
    return new (nk_allocT<NklModule_T>(alloc)) NklModule_T{
        .c = c,
        .mod = nkir_createModule(c->ir),

        .export_set{nullptr, alloc},
    };
}

bool nkl_runModule(NklModule m) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto c = m->c;

    nkl_errorStateEquip(&c->errors);
    defer {
        nkl_errorStateUnequip();
    };

    nk_assert(c->top_level_proc.idx != NKIR_INVALID_IDX);

    if (!nkir_invoke(c->run_ctx, c->top_level_proc, {}, {})) {
        auto const msg = nkir_getRunErrorString(c->run_ctx);
        nkl_reportError(0, 0, NKS_FMT, NKS_ARG(msg));
        return false;
    }

    return true;
}

bool nkl_writeModule(NklModule m, NkIrCompilerConfig conf) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto c = m->c;

    nkl_errorStateEquip(&c->errors);
    defer {
        nkl_errorStateUnequip();
    };

    if (conf.output_kind == NkbOutput_Executable && c->entry_point.idx == NKIR_INVALID_IDX) {
        nkl_reportError(0, 0, "entry point `" ENTRY_POINT_NAME "` either not defined or defined but not exported");
        return false;
    }

    if (!nkir_write(c->ir, m->mod, getNextTempArena(c, NULL), conf)) {
        auto const msg = nkir_getErrorString(c->ir);
        nkl_reportError(0, 0, NKS_FMT, NKS_ARG(msg));
        return false;
    }

    return true;
}

static u32 nodeIdx(NklSource const &src, NklAstNode const &node) {
    return &node - src.nodes.data;
}

struct AstNodeIterator {
    NklSource const &src;
    u32 next_idx;
};

static AstNodeIterator nodeIterate(NklSource const &src, NklAstNode const &node) {
    return {src, nodeIdx(src, node) + 1};
}

static NklAstNode const &nextNode(AstNodeIterator &it) {
    auto const &next_node = it.src.nodes.data[it.next_idx];
    it.next_idx = nkl_ast_nextChild(it.src.nodes, it.next_idx);
    return next_node;
}

// TODO: Figure out exactly what arenas should be used for node and proc stacks
static auto pushNode(Context &ctx, NklAstNode const &node) {
    auto new_stack_node = new (nk_arena_allocT<NodeListNode>(ctx.scope_stack->main_arena)) NodeListNode{
        .next{},
        .node = node,
    };
    nk_list_push(ctx.node_stack, new_stack_node);
    return nk_defer([&ctx]() {
        nk_list_pop(ctx.node_stack);
    });
}

static auto pushProc(Context &ctx, NkIrProc proc) {
    auto const arena = ctx.scope_stack ? ctx.scope_stack->main_arena : getNextTempArena(ctx.c, nullptr);
    auto proc_node = new (nk_arena_allocT<ProcListNode>(arena)) ProcListNode{
        .next{},
        .proc = proc,
        .defer_node{},
        .has_return_in_last_block{},
    };
    nk_list_push(ctx.proc_stack, proc_node);
    auto const prev_proc = nkir_getActiveProc(ctx.ir);
    return nk_defer([&ctx, prev_proc]() {
        nk_list_pop(ctx.proc_stack);
        nkir_setActiveProc(ctx.ir, prev_proc);
    });
}

static char const *typeClassName(NklTypeClass tclass) {
    switch (tclass) {
        case NklType_Any:
            return "any";
        case NklType_Array:
            return "array";
        case NklType_Bool:
            return "bool";
        case NklType_Enum:
            return "enum";
        case NklType_Numeric:
            return "numeric";
        case NklType_Pointer:
            return "pointer";
        case NklType_Procedure:
            return "procedure";
        case NklType_Slice:
            return "slice";
        case NklType_Struct:
            return "struct";
        case NklType_StructPacked:
            return "struct";
        case NklType_Tuple:
            return "tuple";
        case NklType_TuplePacked:
            return "tuple";
        case NklType_Typeref:
            return "type";
        case NklType_Union:
            return "union";

        case NklType_None:
        case NklType_Count:
            break;
    }

    nk_assert(!"unreachable");
    return "";
}

struct CompileConfig {
    nkltype_t res_t = nullptr;
    NklTypeClass res_tclass = NklType_None;
    bool is_const = false;
    Decl *opt_resolved_decl = nullptr;
    NkIrRef dst{};
};

static Interm compileImpl(Context &ctx, NklAstNode const &node, CompileConfig const &conf);
static Void compileStmt(Context &ctx, NklAstNode const &node);

static Interm makeVoid(Context &ctx) {
    return {{}, ctx.c->void_t(), IntermKind_Void};
}

static Interm makeRef(NkIrRef const &ref) {
    return {{.ref{ref}}, nkirt2nklt(ref.type), IntermKind_Ref};
}

template <class T>
static Interm makeConst(Context &ctx, nkltype_t type, T value) {
    auto const rodata = nkir_makeRodata(ctx.ir, 0, nklt2nkirt(type), NkIrVisibility_Local);
    *(T *)nkir_getDataPtr(ctx.ir, rodata) = value;
    return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, type, IntermKind_Val};
}

static Interm makeUsizeConst(Context &ctx, usize value) {
    auto const rodata = nkir_makeRodata(ctx.ir, 0, nklt2nkirt(ctx.c->usize_t()), NkIrVisibility_Local);
    switch (ctx.c->word_size) {
        case 1:
            *(u8 *)nkir_getDataPtr(ctx.ir, rodata) = value;
            break;
        case 2:
            *(u16 *)nkir_getDataPtr(ctx.ir, rodata) = value;
            break;
        case 4:
            *(u32 *)nkir_getDataPtr(ctx.ir, rodata) = value;
            break;
        case 8:
            *(u64 *)nkir_getDataPtr(ctx.ir, rodata) = value;
            break;
        default:
            nk_assert(!"invalid word size");
    }
    return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, ctx.c->usize_t(), IntermKind_Val};
}

template <class T>
static Interm makeNumeric(Context &ctx, nkltype_t num_t, char const *str, char const *fmt) {
    auto const rodata = nkir_makeRodata(ctx.ir, 0, nklt2nkirt(num_t), NkIrVisibility_Local);
    // TODO: Replace sscanf in compiler
    int res = sscanf(str, fmt, (T *)nkir_getDataPtr(ctx.ir, rodata));
    nk_assert(res > 0 && res != EOF && "numeric constant parsing failed");
    return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, num_t, IntermKind_Val};
}

static Interm makeString(Context &ctx, NkString text) {
    auto ar_nt_t = nkl_get_array(ctx.nkl, ctx.c->i8_t(), text.size + 1);
    auto ar_t = nkl_get_array(ctx.nkl, ctx.c->i8_t(), text.size);
    auto str_t = nkl_get_ptr(ctx.nkl, ctx.c->word_size, ar_t, true);

    auto rodata = nkir_makeRodata(ctx.ir, 0, nklt2nkirt(ar_nt_t), NkIrVisibility_Local);
    auto str_ptr = nkir_getDataPtr(ctx.ir, rodata);

    // TODO: Manual copy and null termination
    memcpy(str_ptr, text.data, text.size);
    ((char *)str_ptr)[text.size] = '\0';

    return makeRef(nkir_makeAddressRef(ctx.ir, nkir_makeDataRef(ctx.ir, rodata), nklt2nkirt(str_t)));
}

static Interm makeInstr(NkIrInstr const &instr, nkltype_t type) {
    return {{.instr{instr}}, type, IntermKind_Instr};
}

static Interm store(Context &ctx, NkIrRef const &dst, Interm src) {
    auto const dst_type = dst.type;
    auto const src_type = src.type;
    if (nklt_sizeof(src_type)) {
        if (src.kind == IntermKind_Instr && src.as.instr.arg[0].ref.kind == NkIrRef_None) {
            src.as.instr.arg[0].ref = dst;
        } else {
            src = makeInstr(nkir_make_mov(ctx.ir, dst, asRef(ctx, src)), nkirt2nklt(dst_type));
        }
    }
    auto const src_ref = asRef(ctx, src);
    return makeRef(src_ref);
}

static NkIrRef tupleIndex(NkIrRef ref, nkltype_t tuple_t, usize i) {
    nk_assert(i < nklt_tuple_size(tuple_t));
    ref.type = nklt2nkirt(nklt_tuple_elemType(tuple_t, i));
    ref.post_offset += nklt_tuple_elemOffset(tuple_t, i);
    return ref;
}

static Interm cast(nkltype_t type, Interm val) {
    val.type = type;
    return val;
}

static NkIrRef cast(nkltype_t type, NkIrRef ref) {
    ref.type = nklt2nkirt(type);
    return ref;
}

static Interm compile(Context &ctx, NklAstNode const &node, CompileConfig const conf = {}) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto const pop_node = pushNode(ctx, node);

    DEFINE(val, compileImpl(ctx, node, conf));

    if (conf.is_const && !isValueKnown(val)) {
        // TODO: Improve error message
        return error(ctx, "comptime const expected");
    }

    auto const dst_t = conf.res_t;
    auto const src_t = val.type;

    if (dst_t) {
        if (nklt_tclass(dst_t) == NklType_Slice && nklt_tclass(src_t) == NklType_Pointer &&
            nklt_tclass(nklt_ptr_target(src_t)) == NklType_Array &&
            nklt_typeid(nklt_slice_target(dst_t)) == nklt_typeid(nklt_array_elemType(nklt_ptr_target(src_t)))) {
            if (conf.is_const) {
                val = makeConst(
                    ctx,
                    dst_t,
                    NkString{
                        nklval_as(char const *, getValueFromInterm(ctx, val)),
                        nklt_array_size(nklt_ptr_target(src_t)),
                    });
            } else {
                auto const dst = conf.dst.kind
                                     ? conf.dst
                                     : nkir_makeFrameRef(ctx.ir, nkir_makeLocalVar(ctx.ir, 0, nklt2nkirt(dst_t)));
                auto const struct_t = nklt_underlying(dst_t);
                auto const tuple_t = nklt_underlying(struct_t);
                auto const data_ref = tupleIndex(dst, tuple_t, 0);
                auto const size_ref = tupleIndex(dst, tuple_t, 1);
                store(ctx, data_ref, cast(nklt_struct_field(nklt_underlying(dst_t), 0).type, val));
                store(ctx, size_ref, makeUsizeConst(ctx, nklt_array_size(nklt_ptr_target(src_t))));
                val = makeRef(dst);
            }
        } else {
            // TODO: Distinguish between different types of errors
            bool const can_cast_array_ptr_to_val_ptr =
                nklt_tclass(dst_t) == NklType_Pointer && nklt_tclass(src_t) == NklType_Pointer &&
                nklt_tclass(nklt_ptr_target(src_t)) == NklType_Array &&
                nklt_typeid(nklt_array_elemType(nklt_ptr_target(src_t))) == nklt_typeid(nklt_ptr_target(dst_t)) &&
                (!nklt_ptr_isConst(src_t) || nklt_ptr_isConst(dst_t));

            if (src_t != dst_t && !can_cast_array_ptr_to_val_ptr) {
                return error(
                    ctx,
                    "cannot convert a %s of type '%s' to '%s'",
                    conf.is_const ? "comptime const" : "value",
                    [&]() {
                        NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
                        nkl_type_inspect(src_t, nksb_getStream(&sb));
                        nksb_appendNull(&sb);
                        return sb.data;
                    }(),
                    [&]() {
                        NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
                        nkl_type_inspect(dst_t, nksb_getStream(&sb));
                        nksb_appendNull(&sb);
                        return sb.data;
                    }());
            }

            val.type = dst_t;
        }
    } else if (conf.res_tclass && nklt_tclass(src_t) != conf.res_tclass) {
        // TODO: Improve error message
        return error(ctx, "%s value expected", typeClassName(conf.res_tclass));
    }

    return val;
}

template <class T>
static T compileConst(Context &ctx, NklAstNode const &node, nkltype_t res_t) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    nk_assert(res_t && "must specify res_t for comptime const");

    DEFINE(val, compile(ctx, node, {.res_t = res_t, .is_const = true}));

    return nklval_as(T, getValueFromInterm(ctx, val));
}

static Interm resolveComptime(Decl &decl) {
    nk_assert(decl.kind == DeclKind_Unresolved);

    auto &ctx = *decl.as.unresolved.ctx;
    auto &node = *decl.as.unresolved.node;

    decl.as = {};
    decl.kind = DeclKind_Incomplete;

    auto node_it = nodeIterate(ctx.src, node);

    nextNode(node_it); // name
    auto &type_n = nextNode(node_it);
    auto &val_n = nextNode(node_it);

    if (!type_n.id && !val_n.id) {
        return error(ctx, "invalid ast");
    }

    nkltype_t type{};

    if (type_n.id) {
        ASSIGN(type, compileConst<nkltype_t>(ctx, type_n, ctx.c->type_t()));
    }

    NK_LOG_DBG("Resolving comptime const: node %u file=`%s`", nodeIdx(ctx.src, node), nk_atom2cs(ctx.src.file));

    DEFINE(val, compile(ctx, val_n, {.res_t = type, .is_const = true, .opt_resolved_decl = &decl}));

    // TODO: Do we need this check, or is it always true?
    if (decl.kind != DeclKind_Complete) {
        if (!isValueKnown(val)) {
            // TODO: Improve error message
            return error(ctx, "value is not known");
        }

        decl.as.val = val.as.val;
        decl.kind = DeclKind_Complete;
    }

    return val;
}

static Interm resolveDecl(Context &ctx, Decl &decl) {
    switch (decl.kind) {
        case DeclKind_Unresolved:
            return resolveComptime(decl);
        case DeclKind_Incomplete:
            nk_assert(!"resolveDecl is not implemented for DeclKind_Incomplete");
            return {};
        case DeclKind_Complete:
            return {{.val{decl.as.val}}, getValueType(ctx, decl.as.val), IntermKind_Val};

        case DeclKind_Undefined:
            nk_assert(!"unreachable");
            return {};
    }

    nk_assert(!"unreachable");
    return {};
}

static NkAtom getFileId(NkString filename) {
    NKSB_FIXED_BUFFER(filename_nt, NK_MAX_PATH);
    nksb_printf(&filename_nt, NKS_FMT, NKS_ARG(filename));
    nksb_appendNull(&filename_nt);

    NkString canonical_file_path_s;
    char canonical_file_path[NK_MAX_PATH] = {};
    if (nk_fullPath(canonical_file_path, filename_nt.data) >= 0) {
        canonical_file_path_s = nk_cs2s(canonical_file_path);
    } else {
        canonical_file_path_s = filename;
    }

    return nk_s2atom(canonical_file_path_s);
}

struct CompileParamsConfig {
    bool *allow_variadic_marker;
};

static NklFieldArray compileParams(Context &ctx, NklAstNode const &params_n, CompileParamsConfig const &conf) {
    auto temp_alloc = nk_arena_getAllocator(ctx.scope_stack->temp_arena);
    NkDynArray(NklField) fields{NKDA_INIT(temp_alloc)};

    auto params_it = nodeIterate(ctx.src, params_n);
    for (usize i = 0; i < params_n.arity; i++) {
        auto &param_n = nextNode(params_it);
        auto param_it = nodeIterate(ctx.src, param_n);

        if (conf.allow_variadic_marker && param_n.id == n_ellipsis) {
            if (i != params_n.arity - 1) {
                return error<NklFieldArray>(ctx, "variadic marker must be the last parameter");
            }
            *conf.allow_variadic_marker = true;
            break;
        }

        NklAstNode const *pname_n{};
        NklAstNode const *ptype_n{};
        NklAstNode const *pvalue_n{};
        // TODO: Implement default values for params
        (void)pvalue_n;

        switch (param_n.arity) {
            case 1:
                ptype_n = &nextNode(param_it);
                break;
            case 2:
                pname_n = &nextNode(param_it);
                ptype_n = &nextNode(param_it);
                break;
            case 3:
                pname_n = &nextNode(param_it);
                ptype_n = &nextNode(param_it);
                pvalue_n = &nextNode(param_it);
                break;
            default:
                nk_assert(!"invalid ast");
                return {};
        }

        NkAtom name{};

        if (pname_n) {
            auto name_token = &ctx.src.tokens.data[pname_n->token_idx];
            auto name_str = nkl_getTokenStr(name_token, ctx.src.text);
            name = nk_s2atom(name_str);
        }

        DEFINE(type, compileConst<nkltype_t>(ctx, *ptype_n, ctx.c->type_t()));

        nkda_append(&fields, {name, type});
    }

    return {NK_SLICE_INIT(fields)};
}

static nkltype_t compileProcType(
    Context &ctx,
    NklAstNode const &node,
    NklFieldArray *out_params = nullptr,
    NkCallConv call_conv = NkCallConv_Nk) {
    auto node_it = nodeIterate(ctx.src, node);

    auto &params_n = nextNode(node_it);
    auto &ret_t_n = nextNode(node_it);

    bool variadic_marker_used = false;
    DEFINE(params, compileParams(ctx, params_n, {&variadic_marker_used}))

    if (out_params) {
        *out_params = params;
    }

    DEFINE(ret_t, compileConst<nkltype_t>(ctx, ret_t_n, ctx.c->type_t()));

    u8 const proc_flags = variadic_marker_used ? NkProcVariadic : 0;

    return nkl_get_proc(
        ctx.nkl,
        ctx.c->word_size,
        NklProcInfo{
            .param_types{&params.data->type, params.size, sizeof(params.data[0]) / sizeof(void *)},
            .ret_t = ret_t,
            .call_conv = call_conv,
            .flags = proc_flags,
        });
}

static Void leaveScope(Context &ctx) {
    auto export_node = ctx.scope_stack->export_list;
    while (export_node) {
        // TODO: Avoid additional search for export?
        auto &decl = resolve(ctx, export_node->name);

        nk_assert(decl.kind != DeclKind_Undefined);
        // TODO: Verify that condition
        if (decl.kind == DeclKind_Incomplete) { // error occurred
            return {};
        }

        DEFINE(val, resolveDecl(ctx, decl));

        if (val.as.val.kind == ValueKind_Proc) {
            nkir_exportProc(ctx.ir, ctx.m->mod, val.as.val.as.proc.id);
        } else if (val.as.val.kind == ValueKind_Data) {
            nkir_exportData(ctx.ir, ctx.m->mod, val.as.val.as.data.id);
        } else {
            nk_assert(!"invalid ast");
        }

        export_node = export_node->next;
    }

    popScope(ctx);

    return {};
}

static auto enterPrivateScope(Context &ctx) {
    pushPrivateScope(ctx);
    return nk_defer([&ctx]() {
        emitDefers(ctx, false);
        leaveScope(ctx);
    });
}

static auto enterProcScope(Context &ctx, bool is_public) {
    if (is_public || !ctx.scope_stack) {
        pushPublicScope(ctx);
    } else {
        pushPrivateScope(ctx);
    }
    return nk_defer([&ctx]() {
        emitDefers(ctx, true);
        emit(ctx, nkir_make_ret(ctx.ir));
        leaveScope(ctx);
    });
}

static decltype(Value::as.proc) compileProc(Context &ctx, NkIrProcDescr const &descr, NklAstNode const &body_n) {
    NK_LOG_DBG("Compiling proc %s", descr.name ? nk_atom2cs(descr.name) : "<anonymous>");

    auto const proc = nkir_createProc(ctx.ir);

    // TODO: Handle multiple entry points
    if (descr.name == nk_cs2atom(ENTRY_POINT_NAME)) {
        ctx.c->entry_point = proc;
    }

    Scope *proc_scope = 0;
    {
        auto const pop_proc = pushProc(ctx, proc);
        auto const leave = enterProcScope(ctx, descr.name);

        proc_scope = ctx.scope_stack;

        // TODO: Choose the scope based on the symbol visibility

        nkir_startProc(ctx.ir, proc, descr);

        emit(ctx, nkir_make_label(nkir_createLabel(ctx.ir, nk_cs2atom("@start"))));

        auto arg_names_it = descr.arg_names.data;
        for (usize i = 0; i < descr.arg_names.size; i++) {
            CHECK(defineParam(ctx, *arg_names_it, i));
            arg_names_it = (NkAtom *)((u8 const *)arg_names_it + descr.arg_names.stride);
        }

        CHECK(compileStmt(ctx, body_n));

        // TODO: A very hacky and unstable way to calculate proc finishing line
        u32 proc_finish_line = 0;
        {
            auto const last_node_idx = nkl_ast_nextChild(ctx.src.nodes, nodeIdx(ctx.src, body_n)) - 1;
            if (last_node_idx < ctx.src.nodes.size) {
                auto const &last_node = ctx.src.nodes.data[last_node_idx];
                proc_finish_line = ctx.src.tokens.data[last_node.token_idx].lin + 1;
            }
        }

        nkir_finishProc(ctx.ir, proc, proc_finish_line);
    }

#ifdef ENABLE_LOGGING
    {
        NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(getNextTempArena(ctx.c, nullptr)))};
        nkir_inspectProc(ctx.ir, proc, nksb_getStream(&sb));
        NK_LOG_INF("IR:\n" NKS_FMT, NKS_ARG(sb));
    }
#endif // ENABLE_LOGGING

    return {proc, proc_scope};
}

static Context *importFile(NklCompiler c, NkString filename) {
    auto nkl = c->nkl;

    auto file = getFileId(filename);

    DEFINE(&src, *nkl_getSource(nkl, file));

    auto &pctx = getContextForFile(c, file).val;
    if (!pctx) {
        auto &ctx =
            *(pctx = new (nk_arena_allocT<Context>(&c->perm_arena)) Context{
                  .nkl = nkl,
                  .c = c,
                  .m = nkl_createModule(c),
                  .ir = c->ir,

                  .top_level_proc{},

                  .src = src,

                  .scope_stack{},
                  .node_stack{},
                  .proc_stack{},
              });

        auto proc_t = nkl_get_proc(
            nkl,
            c->word_size,
            NklProcInfo{
                .param_types{},
                .ret_t = c->void_t(),
                .call_conv = NkCallConv_Nk,
                .flags{},
            });

        nk_assert(src.nodes.size && "need at least a null node");

        DEFINE(
            const proc_info,
            compileProc(
                ctx,
                NkIrProcDescr{
                    .name = 0, // TODO: Need to generate names for top level procs
                    .proc_t = nklt2nkirt(proc_t),
                    .arg_names{},
                    .file = src.file,
                    .line = 0,
                    .visibility = NkIrVisibility_Local,
                },
                *src.nodes.data));

        ctx.top_level_proc = proc_info.id;

        // NOTE: Pushing the top-level scope again to leave it accessible for future imports
        nk_list_push(ctx.scope_stack, proc_info.opt_scope);
    }

    return pctx;
}

static Interm getStructIndex(Context &ctx, Interm const &lhs, nkltype_t struct_t, NkAtom name) {
    auto index = nklt_struct_index(struct_t, name);
    if (index == -1u) {
        NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
        nkl_type_inspect(lhs.type, nksb_getStream(&sb));
        return error(ctx, "no field named `%s` in type `" NKS_FMT "`", nk_atom2cs(name), NKS_ARG(sb));
    }
    auto const ref = tupleIndex(asRef(ctx, lhs), nklt_underlying(struct_t), index);
    return makeRef(ref);
}

static Interm deref(NkIrRef ref) {
    auto const ptr_t = nkirt2nklt(ref.type);
    nk_assert(nklt_tclass(ptr_t) == NklType_Pointer);

    auto const target_t = nklt_ptr_target(ptr_t);

    ref.indir++;
    ref.type = nklt2nkirt(target_t);
    return makeRef(ref);
}

static Interm getMember(Context &ctx, Interm const &lhs, NkAtom name) {
    switch (nklt_tclass(lhs.type)) {
        case NklType_Struct:
            return getStructIndex(ctx, lhs, lhs.type, name);

        case NklType_Enum:
        case NklType_Slice:
        case NklType_Any:
            return getStructIndex(ctx, lhs, nklt_underlying(lhs.type), name);

        case NklType_Pointer:
            return getMember(ctx, deref(asRef(ctx, lhs)), name);

        default: {
            NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
            nkl_type_inspect(lhs.type, nksb_getStream(&sb));
            return error(ctx, "type `" NKS_FMT "` is not subscriptable", NKS_ARG(sb));
        }
    }
}

static Interm getIndex(Context &ctx, Interm const &arr, Interm const &idx) {
    switch (nklt_tclass(arr.type)) {
        case NklType_Array: {
            // TODO: Optimize array indexing??
            auto const elem_t = nklt_array_elemType(arr.type);
            auto const data_ptr = asRef(ctx, makeInstr(nkir_make_lea(ctx.ir, {}, asRef(ctx, arr)), ctx.c->usize_t()));
            auto const elem_size = asRef(ctx, makeConst<usize>(ctx, ctx.c->usize_t(), nklt_sizeof(elem_t)));
            auto const mul = nkir_make_mul(ctx.ir, {}, asRef(ctx, idx), elem_size);
            auto const offset = asRef(ctx, makeInstr(mul, ctx.c->usize_t()));
            auto const add = nkir_make_add(ctx.ir, {}, data_ptr, offset);
            auto const elem_ptr_t = nkl_get_ptr(ctx.nkl, ctx.c->word_size, elem_t, false);
            return deref(cast(elem_ptr_t, asRef(ctx, makeInstr(add, ctx.c->usize_t()))));
        }

        default: {
            NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
            nkl_type_inspect(arr.type, nksb_getStream(&sb));
            return error(ctx, "type `" NKS_FMT "` is not indexable", NKS_ARG(sb));
        }
    }
}

// TODO: Provide type in getLvalueRef?
static Interm getLvalueRef(Context &ctx, NklAstNode const &node) {
    auto pop_node = pushNode(ctx, node);

    auto node_it = nodeIterate(ctx.src, node);

    if (node.id == n_id) {
        auto token = &ctx.src.tokens.data[node.token_idx];
        auto name_str = nkl_getTokenStr(token, ctx.src.text);
        auto const name = nk_s2atom(name_str);
        auto &decl = resolve(ctx, name);
        switch (decl.kind) {
            case DeclKind_Undefined:
                return error(ctx, "`" NKS_FMT "` is not declared", NKS_ARG(name_str));

            case DeclKind_Complete:
                switch (decl.as.val.kind) {
                    case ValueKind_Local: {
                        // TODO: Do we need need to immediately convert decl to ref in getLvalueRef?
                        auto const ref = asRef(ctx, resolveDecl(ctx, decl));
                        return makeRef(ref);
                    }

                    case ValueKind_Void:
                    case ValueKind_Rodata:
                    case ValueKind_Proc:
                    case ValueKind_Data:
                    case ValueKind_ExternData:
                    case ValueKind_ExternProc:
                    case ValueKind_Arg:
                        return error(ctx, "cannot assign to `" NKS_FMT "`", NKS_ARG(name_str));
                }
                nk_assert(!"unreachable");
                return {};

            // TODO: Do we need to handle unresolved & incomplete in getLvalueRef?
            case DeclKind_Unresolved:
            case DeclKind_Incomplete:
                return error(ctx, "cannot assign to `" NKS_FMT "`", NKS_ARG(name_str));
        }

        nk_assert(!"unreachable");
        return {};
    } else if (node.id == n_index) {
        auto &arr_n = nextNode(node_it);
        auto &idx_n = nextNode(node_it);

        DEFINE(const arr, compile(ctx, arr_n, {.res_tclass = NklType_Array}));
        DEFINE(const idx, compile(ctx, idx_n, {ctx.c->usize_t()}));

        return getIndex(ctx, arr, idx);
    } else if (node.id == n_member) {
        auto &lhs_n = nextNode(node_it);
        auto &name_n = nextNode(node_it);

        DEFINE(const lhs, compile(ctx, lhs_n));
        auto const name = nk_s2atom(nkl_getTokenStr(&ctx.src.tokens.data[name_n.token_idx], ctx.src.text));

        return getMember(ctx, lhs, name);
    } else if (node.id == n_deref) {
        auto &arg_n = nextNode(node_it);

        DEFINE(const arg, compile(ctx, arg_n, {.res_tclass = NklType_Pointer}));
        return deref(asRef(ctx, arg));
    } else {
        // TODO: Improve error msg
        return error(ctx, "invalid lvalue");
    }
}

static NkAtom getNameFromConstNode(Context &ctx, NklAstNode const &node) {
    if (node.id == n_const) {
        auto parent_it = nodeIterate(ctx.src, node);

        auto &name_n = nextNode(parent_it);
        auto decl_name = nk_s2atom(nkl_getTokenStr(&ctx.src.tokens.data[name_n.token_idx], ctx.src.text));

        return decl_name;
    } else {
        return 0;
    }
}

static NkAtom getConstDeclName(Context &ctx) {
    nk_assert(ctx.node_stack && ctx.node_stack->next && "no parent node");

    auto &parent_n = ctx.node_stack->next->node;
    if (parent_n.id == n_const) {
        return getNameFromConstNode(ctx, parent_n);
    } else if (parent_n.id == n_export && ctx.node_stack->next->next) {
        // TODO: Have to handle a special case for export
        return getNameFromConstNode(ctx, ctx.node_stack->next->next->node);
    } else {
        return 0;
    }
}

static Interm compileImpl(Context &ctx, NklAstNode const &node, CompileConfig const &conf) {
    // TODO: Validate AST

    auto const &token = ctx.src.tokens.data[node.token_idx];

    auto const prev_line = nkir_getLine(ctx.ir);
    defer {
        nkir_setLine(ctx.ir, prev_line);
    };
    nkir_setLine(ctx.ir, token.lin);

    NK_LOG_DBG("Compiling node %u #%s", nodeIdx(ctx.src, node), nk_atom2cs(node.id));

    auto node_it = nodeIterate(ctx.src, node);

    auto const temp_arena = ctx.scope_stack->temp_arena;
    auto const temp_alloc = nk_arena_getAllocator(temp_arena);

    auto const res_t = conf.res_t;

    switch (node.id) {
        case n_null: {
            return makeVoid(ctx);
        }

#define COMPILE_NUM(NAME, IR_NAME)                                                                             \
    case NK_CAT(n_, NAME): {                                                                                   \
        DEFINE(lhs, compile(ctx, nextNode(node_it), {.res_t = res_t, .res_tclass = NklType_Numeric}));         \
        DEFINE(rhs, compile(ctx, nextNode(node_it), {lhs.type}));                                              \
        return makeInstr(NK_CAT(nkir_make_, IR_NAME)(ctx.ir, {}, asRef(ctx, lhs), asRef(ctx, rhs)), lhs.type); \
    }

            COMPILE_NUM(add, add)
            COMPILE_NUM(sub, sub)
            COMPILE_NUM(mul, mul)
            COMPILE_NUM(div, div)
            COMPILE_NUM(mod, mod)

            // TODO: Separate int instructions from the float
            COMPILE_NUM(bitand, and)
            COMPILE_NUM(bitor, or)
            COMPILE_NUM(xor, xor)
            COMPILE_NUM(lsh, lsh)
            COMPILE_NUM(rsh, rsh)

#undef COMPILE_NUM

#define COMPILE_CMP(NAME)                                                                                              \
    case NK_CAT(n_, NAME): {                                                                                           \
        DEFINE(lhs, compile(ctx, nextNode(node_it), {.res_tclass = NklType_Numeric}));                                 \
        DEFINE(rhs, compile(ctx, nextNode(node_it), {lhs.type}));                                                      \
        return makeInstr(NK_CAT(nkir_make_cmp_, NAME)(ctx.ir, {}, asRef(ctx, lhs), asRef(ctx, rhs)), ctx.c->bool_t()); \
    }

            COMPILE_CMP(lt)
            COMPILE_CMP(le)
            COMPILE_CMP(gt)
            COMPILE_CMP(ge)

#undef COMPILE_CMP

#define COMPILE_EQL(NAME)                                                                                              \
    case NK_CAT(n_, NAME): {                                                                                           \
        DEFINE(lhs, compile(ctx, nextNode(node_it)));                                                                  \
        DEFINE(rhs, compile(ctx, nextNode(node_it), {lhs.type}));                                                      \
        return makeInstr(NK_CAT(nkir_make_cmp_, NAME)(ctx.ir, {}, asRef(ctx, lhs), asRef(ctx, rhs)), ctx.c->bool_t()); \
    }

            COMPILE_EQL(eq)
            COMPILE_EQL(ne)

#undef COMPILE_EQL

#define COMPILE_TYPE(NAME, EXPR)                      \
    case NK_CAT(n_, NAME): {                          \
        return makeConst(ctx, ctx.c->type_t(), EXPR); \
    }

            COMPILE_TYPE(any_t, ctx.c->any_t())
            COMPILE_TYPE(bool, ctx.c->bool_t())
            COMPILE_TYPE(type_t, ctx.c->type_t())
            COMPILE_TYPE(usize, ctx.c->usize_t())
            COMPILE_TYPE(void, ctx.c->void_t())

#define X(TYPE, VALUE_TYPE) COMPILE_TYPE(TYPE, ctx.c->NK_CAT(TYPE, _t)())
            NKIR_NUMERIC_ITERATE(X)
#undef X

#undef COMPILE_TYPE

        case n_assign: {
            auto &lhs_n = nextNode(node_it);
            auto &rhs_n = nextNode(node_it);

            DEFINE(lhs, getLvalueRef(ctx, lhs_n));
            DEFINE(rhs, compile(ctx, rhs_n, {lhs.type}));

            return store(ctx, asRef(ctx, lhs), rhs);
        }

        case n_call: {
            auto &lhs_n = nextNode(node_it);
            auto &args_n = nextNode(node_it);

            DEFINE(lhs, compile(ctx, lhs_n, {.res_tclass = NklType_Procedure}));

            auto const param_count = nklt_proc_paramCount(lhs.type);

            NkDynArray(Interm) args{NKDA_INIT(temp_alloc)};

            if (nklt_proc_flags(lhs.type) & NkProcVariadic) {
                if (args_n.arity < param_count) {
                    return error(
                        ctx,
                        "expected at least %zu argument%s, but got %u",
                        param_count,
                        (param_count == 1 ? "" : "s"),
                        args_n.arity);
                }
            } else {
                if (args_n.arity != param_count) {
                    return error(
                        ctx,
                        "expected %zu argument%s, but got %u",
                        param_count,
                        (param_count == 1 ? "" : "s"),
                        args_n.arity);
                }
            }

            auto args_it = nodeIterate(ctx.src, args_n);
            for (usize i = 0; i < args_n.arity; i++) {
                auto &arg_n = nextNode(args_it);
                auto const arg_t = i < param_count ? nklt_proc_paramType(lhs.type, i) : nullptr;
                APPEND(&args, compile(ctx, arg_n, {arg_t}));
            }

            NkDynArray(NkIrRef) arg_refs{NKDA_INIT(temp_alloc)};
            for (usize i = 0; i < args.size; i++) {
                if (i == param_count) {
                    nkda_append(&arg_refs, nkir_makeVariadicMarkerRef(ctx.ir));
                }
                nkda_append(&arg_refs, asRef(ctx, args.data[i]));
            }

            return makeInstr(
                nkir_make_call(ctx.ir, {}, asRef(ctx, lhs), {NK_SLICE_INIT(arg_refs)}), nklt_proc_retType(lhs.type));
        }

        case n_const: {
            auto &name_n = nextNode(node_it);
            auto const name = nk_s2atom(nkl_getTokenStr(&ctx.src.tokens.data[name_n.token_idx], ctx.src.text));
            CHECK(defineComptimeUnresolved(ctx, name, node));
            return makeVoid(ctx);
        }

        case n_context: {
            auto &lhs_n = nextNode(node_it);
            auto &name_n = nextNode(node_it);

            DEFINE(lhs, compile(ctx, lhs_n));

            if (!isModule(lhs)) {
                // TODO: Improve error message
                return error(ctx, "module expected");
            }

            auto scope = getModuleScope(lhs);

            auto name_token = &ctx.src.tokens.data[name_n.token_idx];
            auto name_str = nkl_getTokenStr(name_token, ctx.src.text);
            auto const name = nk_s2atom(name_str);

            NK_LOG_DBG("Resolving id: name=`%s` scope=%p", nk_atom2cs(name), (void *)scope);

            auto found = DeclMap_find(&scope->locals, name);
            if (found) {
                return resolveDecl(ctx, found->val);
            } else {
                return error(ctx, "`" NKS_FMT "` is not declared", NKS_ARG(name_str));
            }

            break;
        }

        case n_link: {
            auto const decl_name = getConstDeclName(ctx);
            if (!decl_name) {
                return error(ctx, "invalid ast");
            }

            nk_assert(conf.opt_resolved_decl && "invalid ast");

            auto &lib_n = nextNode(node_it);
            auto &proc_n = nextNode(node_it);

            if (proc_n.id != n_proc) {
                return error(ctx, "invalid ast");
            }

            DEFINE(lib, compileConst<NkString>(ctx, lib_n, ctx.c->str_t()));

            if (nks_equal(lib, nk_cs2s("c")) || nks_equal(lib, nk_cs2s("C"))) {
                // TODO: Using hard-configured libc name
                lib = nk_cs2s(LIBC_NAME);
            }

            DEFINE(const proc_t, compileProcType(ctx, proc_n, nullptr, NkCallConv_Cdecl));

            // TODO: Using hard-configured libc name
            auto const proc = nkir_makeExternProc(ctx.ir, nk_s2atom(lib), decl_name, nklt2nkirt(proc_t));

            return {{.val{{.extern_proc{proc}}, ValueKind_ExternProc}}, proc_t, IntermKind_Val};
        }

        case n_id: {
            auto token = &ctx.src.tokens.data[node.token_idx];
            auto name_str = nkl_getTokenStr(token, ctx.src.text);
            auto const name = nk_s2atom(name_str);

            auto &decl = resolve(ctx, name);

            // TODO: Handle cross frame references

            if (decl.kind == DeclKind_Undefined) {
                return error(ctx, "`" NKS_FMT "` is not declared", NKS_ARG(name_str));
            } else {
                return resolveDecl(ctx, decl);
            }
        }

        case n_import: {
            auto &path_n = nextNode(node_it);
            auto path_str = nkl_getTokenStr(&ctx.src.tokens.data[path_n.token_idx], ctx.src.text);
            NkString const path{path_str.data + 1, path_str.size - 2};

            NKSB_FIXED_BUFFER(path_buf, NK_MAX_PATH);
            nksb_printf(&path_buf, NKS_FMT, NKS_ARG(path));
            nksb_appendNull(&path_buf);

            NkString import_path{};

            if (nk_pathIsRelative(path_buf.data)) {
                auto parent = nk_path_getParent(nk_atom2s(ctx.src.file));

                nksb_clear(&path_buf);
                nksb_printf(&path_buf, NKS_FMT "%c" NKS_FMT, NKS_ARG(parent), NK_PATH_SEPARATOR, NKS_ARG(path));

                import_path = {NKS_INIT(path_buf)};
            } else {
                import_path = path;
            }

            DEFINE(&imported_ctx, *importFile(ctx.c, import_path));

            return {
                {.val{{.proc{imported_ctx.top_level_proc, imported_ctx.scope_stack}}, ValueKind_Proc}},
                nkirt2nklt(nkir_getProcType(ctx.ir, imported_ctx.top_level_proc)),
                IntermKind_Val};
        }

        case n_int: {
            auto const value_type =
                (res_t && nklt_tclass(res_t) == NklType_Numeric) ? nklt_numeric_valueType(res_t) : Int64;
            auto const token_str = nkl_getTokenStr(&ctx.src.tokens.data[node.token_idx], ctx.src.text);
            switch (value_type) {
#define X(TYPE, VALUE_TYPE) \
    case VALUE_TYPE:        \
        return makeNumeric<TYPE>(ctx, ctx.c->NK_CAT(TYPE, _t)(), token_str.data, "%" NK_CAT(SCN, TYPE));
                NKIR_NUMERIC_ITERATE(X)
#undef X
            }
            nk_assert(!"unreachable");
            return {};
        }

        case n_float: {
            auto const value_type =
                (res_t && nklt_tclass(res_t) == NklType_Numeric) ? nklt_numeric_valueType(res_t) : Float64;
            auto const token_str = nkl_getTokenStr(&ctx.src.tokens.data[node.token_idx], ctx.src.text);
            switch (value_type) {
                case Float32:
                    return makeNumeric<f32>(ctx, ctx.c->f32_t(), token_str.data, "%" SCNf32);
                default:
                case Float64:
                    return makeNumeric<f64>(ctx, ctx.c->f64_t(), token_str.data, "%" SCNf64);
            }
        }

        case n_false: {
            return makeConst(ctx, ctx.c->bool_t(), false);
        }

        case n_true: {
            return makeConst(ctx, ctx.c->bool_t(), true);
        }

        case n_nullptr: {
            return makeConst<void *>(
                ctx, (res_t && nklt_tclass(res_t) == NklType_Pointer) ? res_t : ctx.c->void_t(), nullptr);
        }

        case n_list: {
            for (usize i = 0; i < node.arity; i++) {
                CHECK(compileStmt(ctx, nextNode(node_it)));
            }
            return makeVoid(ctx);
        }

        case n_addr: {
            DEFINE(arg, compile(ctx, nextNode(node_it)));
            return makeInstr(
                nkir_make_lea(ctx.ir, {}, asRef(ctx, arg)), nkl_get_ptr(ctx.nkl, ctx.c->word_size, arg.type, false));
        }

        case n_deref:
        case n_index:
        case n_member:
            return getLvalueRef(ctx, node);

        case n_export: {
            auto &const_n = nextNode(node_it);

            if (const_n.id != n_const) {
                return error(ctx, "invalid ast");
            }

            auto const_it = nodeIterate(ctx.src, const_n);

            auto &name_n = nextNode(const_it);
            auto const name = nk_s2atom(nkl_getTokenStr(&ctx.src.tokens.data[name_n.token_idx], ctx.src.text));

            auto export_node = new (nk_allocT<NkAtomListNode>(temp_alloc)) NkAtomListNode{
                .next{},
                .name = name,
            };
            nk_list_push(ctx.scope_stack->export_list, export_node);

            auto const found = NkAtomSet_find(&ctx.m->export_set, name);
            if (found) {
                // TODO: Report the conflicting export location
                return error(ctx, "symbol '%s' is already exported in the current module", nk_atom2cs(name));
            }
            NkAtomSet_insert(&ctx.m->export_set, name);

            DEFINE(proc, compile(ctx, const_n));

            return proc;
        }

        case n_proc: {
            if (node.arity == 2) {
                DEFINE(const proc_t, compileProcType(ctx, node));
                return makeConst(ctx, ctx.c->type_t(), proc_t);
            } else if (node.arity == 3) {
                auto const decl_name = getConstDeclName(ctx);

                nextNode(node_it); // params
                nextNode(node_it); // ret_t
                auto &body_n = nextNode(node_it);

                NklFieldArray params{};
                DEFINE(const proc_t, compileProcType(ctx, node, &params));

                DEFINE(
                    const proc_info,
                    compileProc(
                        ctx,
                        NkIrProcDescr{
                            .name = decl_name, // TODO: Need to generate names for anonymous procs
                            .proc_t = nklt2nkirt(proc_t),
                            .arg_names{&params.data->name, params.size, sizeof(params.data[0])},
                            .file = ctx.src.file,
                            .line = token.lin,
                            .visibility = NkIrVisibility_Default, // TODO: Hardcoded visibility
                        },
                        body_n));

                return {{.val{{.proc{proc_info}}, ValueKind_Proc}}, proc_t, IntermKind_Val};
            } else {
                return error(ctx, "invalid ast");
            }
        }

        case n_ptr: {
            auto ptarget_n = &nextNode(node_it);
            bool const is_const = ptarget_n->id == n_const;
            if (is_const) {
                auto it = nodeIterate(ctx.src, *ptarget_n);
                ptarget_n = &nextNode(it);
            }
            DEFINE(target_t, compileConst<nkltype_t>(ctx, *ptarget_n, ctx.c->type_t()));
            return makeConst(ctx, ctx.c->type_t(), nkl_get_ptr(ctx.nkl, ctx.c->word_size, target_t, is_const));
        }

        case n_slice: {
            auto ptarget_n = &nextNode(node_it);
            bool const is_const = ptarget_n->id == n_const;
            if (is_const) {
                auto it = nodeIterate(ctx.src, *ptarget_n);
                ptarget_n = &nextNode(it);
            }
            DEFINE(target_t, compileConst<nkltype_t>(ctx, *ptarget_n, ctx.c->type_t()));
            return makeConst(ctx, ctx.c->type_t(), nkl_get_slice(ctx.nkl, ctx.c->word_size, target_t, is_const));
        }

        case n_return: {
            emitDefers(ctx, true);
            if (node.arity) {
                auto &arg_n = nextNode(node_it);
                auto const ret_t = nklt_proc_retType(nkirt2nklt(nkir_getProcType(ctx.ir, ctx.proc_stack->proc)));
                DEFINE(arg, compile(ctx, arg_n, {ret_t}));
                emit(ctx, nkir_make_mov(ctx.ir, nkir_makeRetRef(ctx.ir), asRef(ctx, arg)));
            }
            emit(ctx, nkir_make_ret(ctx.ir));

            return makeVoid(ctx);
        }

        case n_run: {
            auto &ret_t_n = nextNode(node_it);
            auto &body_n = nextNode(node_it);

            nkltype_t ret_t{};
            if (ret_t_n.id) {
                ASSIGN(ret_t, compileConst<nkltype_t>(ctx, ret_t_n, ctx.c->type_t()));
            } else {
                ret_t = ctx.c->void_t();
            }

            auto proc_t = nkl_get_proc(
                ctx.nkl,
                ctx.c->word_size,
                NklProcInfo{
                    .param_types{},
                    .ret_t = ret_t,
                    .call_conv = NkCallConv_Nk,
                    .flags{},
                });

            DEFINE(
                const proc_info,
                compileProc(
                    ctx,
                    NkIrProcDescr{
                        .name = nk_cs2atom("__comptime"), // TODO: Hardcoded comptime proc name
                        .proc_t = nklt2nkirt(proc_t),
                        .arg_names{},
                        .file = ctx.src.file,
                        .line = token.lin,
                        .visibility = NkIrVisibility_Local,
                    },
                    body_n));

            void *rets[] = {nullptr};
            NkIrData rodata{};

            if (nklt_sizeof(ret_t)) {
                rodata = nkir_makeRodata(ctx.ir, 0, nklt2nkirt(ret_t), NkIrVisibility_Local);
                rets[0] = {nkir_getDataPtr(ctx.ir, rodata)};
            }

            if (!nkir_invoke(ctx.c->run_ctx, proc_info.id, {}, rets)) {
                auto const msg = nkir_getRunErrorString(ctx.c->run_ctx);
                nkl_reportError(ctx.src.file, &token, NKS_FMT, NKS_ARG(msg));
                return {};
            }

            if (nklt_sizeof(ret_t)) {
                return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, ret_t, IntermKind_Val};
            } else {
                return makeVoid(ctx);
            }
        }

        case n_scope: {
            auto const leave = enterPrivateScope(ctx);

            auto &child_n = nextNode(node_it);
            CHECK(compileStmt(ctx, child_n));

            return makeVoid(ctx);
        }

        case n_string: {
            auto const token_str = nkl_getTokenStr(&ctx.src.tokens.data[node.token_idx], ctx.src.text);
            NkString const text{token_str.data + 1, token_str.size - 2};

            return makeString(ctx, text);
        }

        case n_escaped_string: {
            auto const token_str = nkl_getTokenStr(&ctx.src.tokens.data[node.token_idx], ctx.src.text);
            NkString const text{token_str.data + 1, token_str.size - 2};

            NkStringBuilder sb{NKSB_INIT(temp_alloc)};
            nks_unescape(nksb_getStream(&sb), text);
            NkString const unsescaped_text{NKS_INIT(sb)};

            return makeString(ctx, unsescaped_text);
        }

        case n_struct: {
            auto &params_n = nextNode(node_it);
            DEFINE(fields, compileParams(ctx, params_n, {}));
            auto struct_t = nkl_get_struct(ctx.nkl, {NK_SLICE_INIT(fields)});
            return makeConst(ctx, ctx.c->type_t(), struct_t);
        }

        case n_array: {
            switch (node.arity) {
                case 1: {
                    return error(ctx, "TODO: array literal is not implemented");
                }
                case 2: {
                    auto &size_n = nextNode(node_it);
                    auto &type_n = nextNode(node_it);

                    // TODO: Doing a reinterpret cast to the host usize for cross-compilation use case
                    DEFINE(size, compileConst<usize>(ctx, size_n, ctx.c->usize_t()));
                    DEFINE(type, compileConst<nkltype_t>(ctx, type_n, ctx.c->type_t()));

                    auto array_t = nkl_get_array(ctx.nkl, type, size);
                    return makeConst(ctx, ctx.c->type_t(), array_t);
                }
            }
            nk_assert(!"invalid ast");
            return {};
        }

        case n_var: {
            auto &name_n = nextNode(node_it);
            auto &type_n = nextNode(node_it);
            NklAstNode const *pval_n{};

            switch (node.arity) {
                case 2:
                    break;
                case 3:
                    pval_n = &nextNode(node_it);
                    break;
                default:
                    nk_assert(!"invalid ast");
                    return {};
            }

            if (!type_n.id && !pval_n) {
                return error(ctx, "invalid ast");
            }

            auto const name = nk_s2atom(nkl_getTokenStr(&ctx.src.tokens.data[name_n.token_idx], ctx.src.text));

            nkltype_t type{};

            if (type_n.id) {
                ASSIGN(type, compileConst<nkltype_t>(ctx, type_n, ctx.c->type_t()));
            }

            NkIrLocalVar var{};
            NkIrRef ref{};

            if (type) {
                var = nkir_makeLocalVar(ctx.ir, name, nklt2nkirt(type));
                ref = nkir_makeFrameRef(ctx.ir, var);
            }

            Interm val{};
            if (pval_n) {
                ASSIGN(val, compile(ctx, *pval_n, {.res_t = type, .dst = ref}));
                type = val.type;
                if (!ref.kind) {
                    var = nkir_makeLocalVar(ctx.ir, name, nklt2nkirt(type));
                    ref = nkir_makeFrameRef(ctx.ir, var);
                    store(ctx, ref, val);
                }
            }

            CHECK(defineLocal(ctx, name, var));

            return makeVoid(ctx);
        }

        case n_if: {
            auto &cond_n = nextNode(node_it);
            auto &body_n = nextNode(node_it);
            auto &else_n = nextNode(node_it);

            auto const endif_l = nkir_createLabel(ctx.ir, nk_cs2atom("@endif"));
            NkIrLabel else_l;
            if (else_n.id) {
                else_l = nkir_createLabel(ctx.ir, nk_cs2atom("@else"));
            } else {
                else_l = endif_l;
            }

            DEFINE(cond, compile(ctx, cond_n, {ctx.c->bool_t()}));

            emit(ctx, nkir_make_jmpz(ctx.ir, asRef(ctx, cond), else_l));

            {
                auto const leave = enterPrivateScope(ctx);
                CHECK(compileStmt(ctx, body_n));
            }

            if (else_n.id) {
                emit(ctx, nkir_make_label(else_l));
                {
                    auto const leave = enterPrivateScope(ctx);
                    CHECK(compileStmt(ctx, else_n));
                }
            }

            emit(ctx, nkir_make_label(endif_l));

            return makeVoid(ctx);
        }

        case n_while: {
            auto &cond_n = nextNode(node_it);
            auto &body_n = nextNode(node_it);

            auto const loop_l = nkir_createLabel(ctx.ir, nk_cs2atom("@loop"));
            auto const endloop_l = nkir_createLabel(ctx.ir, nk_cs2atom("@endloop"));

            emit(ctx, nkir_make_label(loop_l));

            DEFINE(cond, compile(ctx, cond_n, {ctx.c->bool_t()}));

            emit(ctx, nkir_make_jmpz(ctx.ir, asRef(ctx, cond), endloop_l));

            {
                auto const leave = enterPrivateScope(ctx);
                CHECK(compileStmt(ctx, body_n));
            }

            emit(ctx, nkir_make_jmp(ctx.ir, loop_l));
            emit(ctx, nkir_make_label(endloop_l));

            return makeVoid(ctx);
        }

        case n_defer_stmt: {
            auto &stmt_n = nextNode(node_it);

            auto const defer_node = new (nk_allocT<DeferListNode>(temp_alloc)) DeferListNode{
                .next{},
                .instrs{NKDA_INIT(temp_alloc)},
                .file = ctx.src.file,
                .node_idx = nodeIdx(ctx.src, node),
            };

            {
                auto const prev_defer_node = ctx.proc_stack->defer_node;
                ctx.proc_stack->defer_node = defer_node;
                defer {
                    ctx.proc_stack->defer_node = prev_defer_node;
                };

#ifdef ENABLE_LOGGING
                NK_LOG_DBG(
                    "Defer recording start for node %u file=`%s`", defer_node->node_idx, nk_atom2cs(defer_node->file));
                emit(
                    ctx,
                    nkir_make_comment(ctx.ir, nk_tsprintf(temp_arena, "begin defer node %u", defer_node->node_idx)));
                defer {
                    emit(
                        ctx,
                        nkir_make_comment(ctx.ir, nk_tsprintf(temp_arena, "end defer node %u", defer_node->node_idx)));
                    NK_LOG_DBG(
                        "Defer recording finish for node %u file=`%s`",
                        defer_node->node_idx,
                        nk_atom2cs(defer_node->file));
                };
#endif // ENABLE_LOGGING

                CHECK(compileStmt(ctx, stmt_n));
            }

            nk_list_push(ctx.scope_stack->defer_stack, defer_node);

            return makeVoid(ctx);
        }

        default: {
            return error(ctx, "unknown AST node #%s", nk_atom2cs(node.id));
        }
    }

    nk_assert(!"unreachable");
    return {};
}

static Void compileStmt(Context &ctx, NklAstNode const &node) {
    DEFINE(val, compile(ctx, node));
    auto const ref = asRef(ctx, val);
    if (ref.kind != NkIrRef_None && ref.type->size) {
        NKSB_FIXED_BUFFER(sb, 1024);
        nk_assert(ctx.proc_stack && "no current proc");
        nkir_inspectRef(ctx.ir, ctx.proc_stack->proc, ref, nksb_getStream(&sb));
        NK_LOG_DBG("Value ignored: " NKS_FMT, NKS_ARG(sb));
    }

    return {};
}

bool nkl_compileFile(NklModule m, NkString filename) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto c = m->c;

    nkl_errorStateEquip(&c->errors);
    defer {
        nkl_errorStateUnequip();
    };

    DEFINE(&ctx, *importFile(c, filename));

    // TODO: Storing top level proc for the whole compiler on every compileFile call
    c->top_level_proc = ctx.top_level_proc;

    // TODO: Check for export conflicts when merging modules
    nkir_mergeModules(m->mod, ctx.m->mod);

#ifdef ENABLE_LOGGING
    {
        NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
        auto const out = nksb_getStream(&sb);
        nkir_inspectData(c->ir, out);
        nkir_inspectExternSyms(c->ir, out);
        NK_LOG_INF("IR:\n" NKS_FMT, NKS_ARG(sb));
    }
#endif // ENABLE_LOGGING

    return true;
}
