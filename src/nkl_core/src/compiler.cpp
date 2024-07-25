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

} // namespace

NklCompiler nkl_createCompiler(NklState nkl, NklTargetTriple target) {
    NkArena arena{};
    auto c = new (nk_arena_allocT<NklCompiler_T>(&arena)) NklCompiler_T{
        .ir{},
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
    return new (nk_allocT<NklModule_T>(nk_arena_getAllocator(&c->perm_arena))) NklModule_T{
        .c = c,
        .mod = nkir_createModule(c->ir),
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

    nk_assert(c->entry_point.idx != NKIR_INVALID_IDX);

    if (!nkir_invoke(c->run_ctx, c->entry_point, {}, {})) {
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

    if (!nkir_write(c->ir, m->mod, getNextTempArena(c, NULL), conf)) {
        auto const msg = nkir_getErrorString(c->ir);
        nkl_reportError(0, 0, NKS_FMT, NKS_ARG(msg));
        return false;
    }

    return true;
}

NK_PRINTF_LIKE(2) static Interm error(Context &ctx, char const *fmt, ...) {
    auto &src = ctx.src;

    auto last_node = ctx.node_stack;
    auto err_token = last_node ? &src.tokens.data[last_node->node.token_idx] : nullptr;

    va_list ap;
    va_start(ap, fmt);
    nkl_vreportError(src.file, err_token, fmt, ap);
    va_end(ap);

    return {};
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

static Interm compile(Context &ctx, NklAstNode const &node, nkltype_t type = nullptr);
static Void compileStmt(Context &ctx, NklAstNode const &node);

static Interm resolveComptime(Decl &decl) {
    nk_assert(decl.kind == DeclKind_Unresolved);

    auto &ctx = *decl.as.unresolved.ctx;
    auto &node = *decl.as.unresolved.node;

    decl.as = {};
    decl.kind = DeclKind_Incomplete;

    NK_LOG_DBG("Resolving comptime const: node %u file=`%s`", nodeIdx(ctx.src, node), nk_atom2cs(ctx.src.file));

    DEFINE(val, compile(ctx, node)); // TODO: Add possibility to specify type for const

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

static Interm resolveDecl(NklCompiler c, Decl &decl) {
    switch (decl.kind) {
        case DeclKind_Unresolved:
            return resolveComptime(decl);
        case DeclKind_Incomplete:
            nk_assert(!"resolveDecl is not implemented for DeclKind_Incomplete");
            return {};
        case DeclKind_Complete:
            return {{.val{decl.as.val}}, getValueType(c, decl.as.val), IntermKind_Val};

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

using NkAtomDynArray = NkDynArray(NkAtom);

static nkltype_t compileProcType(
    Context &ctx,
    NklAstNode const &node,
    NkAtomDynArray *out_param_names = nullptr,
    NkCallConv call_conv = NkCallConv_Nk) {
    auto &src = ctx.src;
    auto m = ctx.m;
    auto c = m->c;
    auto nkl = c->nkl;

    auto node_it = nodeIterate(src, node);

    auto &params_n = nextNode(node_it);
    auto &ret_t_n = nextNode(node_it);

    auto temp_alloc = nk_arena_getAllocator(ctx.scope_stack->temp_arena);

    NkDynArray(nkltype_t) param_types{NKDA_INIT(temp_alloc)};

    u8 proc_flags = 0;

    auto params_it = nodeIterate(src, params_n);
    for (usize i = 0; i < params_n.arity; i++) {
        auto &param_n = nextNode(params_it);
        auto param_it = nodeIterate(src, param_n);

        if (param_n.id == n_ellipsis) {
            proc_flags |= NkProcVariadic;
            // TODO: Check that ellipsis is the last param
            break;
        }

        auto &name_n = nextNode(param_it);
        auto &type_n = nextNode(param_it);

        NkAtom name{};

        if (name_n.id) {
            auto name_token = &src.tokens.data[name_n.token_idx];
            auto name_str = nkl_getTokenStr(name_token, src.text);
            name = nk_s2atom(name_str);
        }

        DEFINE(type_v, compile(ctx, type_n, nkl_get_typeref(nkl, c->word_size)));

        if (type_v.type->tclass != NklType_Typeref) {
            // TODO: Improve error message
            return error(ctx, "type expected"), nkltype_t{};
        }

        if (!isValueKnown(type_v)) {
            // TODO: Improve error message
            return error(ctx, "value is not known"), nkltype_t{};
        }

        auto type = nklval_as(nkltype_t, getValueFromInterm(c, type_v));

        if (out_param_names) {
            nkda_append(out_param_names, name);
        }
        nkda_append(&param_types, type);
    }

    DEFINE(ret_t_v, compile(ctx, ret_t_n, nkl_get_typeref(nkl, c->word_size)));

    // TODO: Boilerplate in type checking
    if (ret_t_v.type->tclass != NklType_Typeref) {
        // TODO: Improve error message
        return error(ctx, "type expected"), nkltype_t{};
    }

    if (!isValueKnown(ret_t_v)) {
        // TODO: Improve error message
        return error(ctx, "value is not known"), nkltype_t{};
    }

    auto ret_t = nklval_as(nkltype_t, getValueFromInterm(c, ret_t_v));

    return nkl_get_proc(
        nkl,
        c->word_size,
        NklProcInfo{
            .param_types{NK_SLICE_INIT(param_types)},
            .ret_t = ret_t,
            .call_conv = call_conv,
            .flags = proc_flags,
        });
}

static decltype(Value::as.proc) compileProc(Context &ctx, NkIrProcDescr const &descr, NklAstNode const &body_n) {
    auto &src = ctx.src;
    auto m = ctx.m;
    auto c = m->c;

    auto const proc = nkir_createProc(c->ir);
    if (descr.visibility != NkIrVisibility_Local) {
        nkir_exportProc(c->ir, m->mod, proc);
    }

    auto const prev_proc = nkir_getActiveProc(c->ir);
    defer {
        nkir_setActiveProc(c->ir, prev_proc);
    };

    nkir_startProc(c->ir, proc, descr);

    nkir_emit(c->ir, nkir_make_label(nkir_createLabel(c->ir, nk_cs2atom("@start"))));

    if (descr.name || !ctx.scope_stack) {
        pushPublicScope(ctx, proc);
    } else {
        pushPrivateScope(ctx, proc);
    }
    defer {
        popScope(ctx);
    };

    for (usize i = 0; i < descr.arg_names.size; i++) {
        CHECK(defineParam(ctx, descr.arg_names.data[i], i));
    }

    CHECK(compileStmt(ctx, body_n));

    // TODO: A very hacky and unstable way to calculate proc finishing line
    u32 proc_finish_line = 0;
    {
        auto const last_node_idx = nkl_ast_nextChild(src.nodes, nodeIdx(src, body_n)) - 1;
        if (last_node_idx < src.nodes.size) {
            auto const &last_node = src.nodes.data[last_node_idx];
            proc_finish_line = src.tokens.data[last_node.token_idx].lin + 1;
        }
    }

    nkir_emit(c->ir, nkir_make_ret(c->ir));

    nkir_finishProc(c->ir, proc, proc_finish_line);

#ifdef ENABLE_LOGGING
    {
        NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
        nkir_inspectProc(c->ir, proc, nksb_getStream(&sb));
        NK_LOG_INF("IR:\n" NKS_FMT, NKS_ARG(sb));
    }
#endif // ENABLE_LOGGING

    return {proc, ctx.scope_stack};
}

static Context *importFile(NklCompiler c, NkString filename) {
    auto nkl = c->nkl;

    auto file = getFileId(filename);

    DEFINE(&src, *nkl_getSource(nkl, file));

    auto &pctx = getContextForFile(c, file).val;
    if (!pctx) {
        auto &ctx =
            *(pctx = new (nk_arena_allocT<Context>(&c->perm_arena)) Context{
                  .top_level_proc{},
                  .m = nkl_createModule(c),
                  .src = src,
                  .scope_stack{},
                  .node_stack{},
              });

        auto proc_t = nkl_get_proc(
            nkl,
            c->word_size,
            NklProcInfo{
                .param_types{},
                .ret_t = nkl_get_void(nkl),
                .call_conv = NkCallConv_Nk,
                .flags{},
            });

        nk_assert(src.nodes.size && "need at least a null node");

        auto const proc_info = compileProc(
            ctx,
            NkIrProcDescr{
                .name = 0, // TODO: Hardcoded toplevel proc name
                .proc_t = nklt2nkirt(proc_t),
                .arg_names{},
                .file = src.file,
                .line = 0,
                .visibility = NkIrVisibility_Local,
            },
            *src.nodes.data);

        ctx.top_level_proc = proc_info.id;

        // NOTE: Pushing the top-level scope again to leave it accessible for future imports
        nk_list_push(ctx.scope_stack, proc_info.opt_scope);
    }

    return pctx;
}

static Interm store(Context &ctx, NkIrRef const &dst, Interm src) {
    auto m = ctx.m;
    auto c = m->c;

    auto const dst_type = dst.type;
    auto const src_type = src.type;
    if (nklt_sizeof(src_type)) {
        if (src.kind == IntermKind_Instr && src.as.instr.arg[0].ref.kind == NkIrRef_None) {
            src.as.instr.arg[0].ref = dst;
        } else {
            src = {{.instr{nkir_make_mov(c->ir, dst, asRef(ctx, src))}}, nkirt2nklt(dst_type), IntermKind_Instr};
        }
    }
    auto const src_ref = asRef(ctx, src);
    return {{.ref = src_ref}, nkirt2nklt(src_ref.type), IntermKind_Ref};
}

static NkIrRef tupleIndex(NkIrRef ref, nkltype_t tuple_t, usize i) {
    nk_assert(i < tuple_t->ir_type.as.aggr.elems.size);
    ref.type = tuple_t->ir_type.as.aggr.elems.data[i].type;
    ref.post_offset += tuple_t->ir_type.as.aggr.elems.data[i].offset;
    return ref;
}

Interm getStructIndex(Context &ctx, Interm const &lhs, nkltype_t struct_t, NkAtom name) {
    auto index = nklt_struct_index(struct_t, name);
    if (index == -1u) {
        NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
        nkl_type_inspect(lhs.type, nksb_getStream(&sb));
        return error(ctx, "no field named `%s` in type `" NKS_FMT "`", nk_atom2cs(name), NKS_ARG(sb));
    }
    auto const ref = tupleIndex(asRef(ctx, lhs), struct_t->underlying_type, index);
    return {{.ref{ref}}, nkirt2nklt(ref.type), IntermKind_Ref};
}

static Interm getMember(Context &ctx, Interm const &lhs, NkAtom name) {
    switch (nklt_tclass(lhs.type)) {
        case NklType_Struct:
            return getStructIndex(ctx, lhs, lhs.type, name);

        default: {
            NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
            nkl_type_inspect(lhs.type, nksb_getStream(&sb));
            return error(ctx, "type `" NKS_FMT "` is not subscriptable", NKS_ARG(sb));
        }
    }
}

static Interm deref(NkIrRef ref) {
    auto const ptr_t = nkirt2nklt(ref.type);
    nk_assert(nklt_tclass(ptr_t) == NklType_Pointer);

    auto const target_t = ptr_t->as.ptr.target_type;

    ref.indir++;
    ref.type = nklt2nkirt(target_t);
    return {{.ref{ref}}, nkirt2nklt(ref.type), IntermKind_Ref};
}

static auto pushNode(Context &ctx, NklAstNode const &node, nkltype_t type) {
    auto new_stack_node = new (nk_arena_allocT<NodeListNode>(ctx.scope_stack->main_arena)) NodeListNode{
        .next{},
        .node = node,
        .type = type,
    };
    nk_list_push(ctx.node_stack, new_stack_node);
    return nk_defer([&ctx]() {
        nk_list_pop(ctx.node_stack);
    });
}

static Interm getLvalueRef(Context &ctx, NklAstNode const &node) {
    auto pop_node = pushNode(ctx, node, nullptr); // TODO: Provide type in getLvalueRef

    auto &src = ctx.src;
    auto m = ctx.m;
    auto c = m->c;

    if (node.id == n_id) {
        auto token = &src.tokens.data[node.token_idx];
        auto name_str = nkl_getTokenStr(token, src.text);
        auto name = nk_s2atom(name_str);
        auto &decl = resolve(ctx, name);
        switch (decl.kind) {
            case DeclKind_Undefined:
                return error(ctx, "`" NKS_FMT "` is not defined", NKS_ARG(name_str));

            case DeclKind_Complete:
                switch (decl.as.val.kind) {
                    case ValueKind_Local: {
                        // TODO: Do we need need to immediately convert decl to ref in getLvalueRef?
                        auto const ref = asRef(ctx, resolveDecl(c, decl));
                        return {{.ref{ref}}, nkirt2nklt(ref.type), IntermKind_Ref};
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
    } else if (node.id == n_member) {
        auto node_it = nodeIterate(src, node);

        auto &lhs_n = nextNode(node_it);
        auto &name_n = nextNode(node_it);

        DEFINE(const lhs, compile(ctx, lhs_n));

        auto const name = nk_s2atom(nkl_getTokenStr(&src.tokens.data[name_n.token_idx], src.text));

        return getMember(ctx, lhs, name);
    } else if (node.id == n_deref) {
        auto node_it = nodeIterate(src, node);

        auto &arg_n = nextNode(node_it);

        DEFINE(const arg, compile(ctx, arg_n));
        if (arg.type->tclass != NklType_Pointer) {
            return error(ctx, "pointer expected in dereference");
        }
        return deref(asRef(ctx, arg));
    } else {
        // TODO: Improve error msg
        return error(ctx, "invalid lvalue");
    }
}

static NkAtom getConstDeclName(Context &ctx) {
    nk_assert(ctx.node_stack && ctx.node_stack->next && "no parent node");

    auto &parent_n = ctx.node_stack->next->node;
    if (parent_n.id == n_const) {
        auto &src = ctx.src;

        auto parent_it = nodeIterate(src, parent_n);

        auto &name_n = nextNode(parent_it);
        auto decl_name = nk_s2atom(nkl_getTokenStr(&src.tokens.data[name_n.token_idx], src.text));

        return decl_name;
    } else {
        return 0;
    }
}

static Interm compile(Context &ctx, NklAstNode const &node, nkltype_t res_type) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto &src = ctx.src;
    auto m = ctx.m;
    auto c = m->c;
    auto nkl = c->nkl;

    auto temp_frame = nk_arena_grab(ctx.scope_stack->temp_arena);
    defer {
        nk_arena_popFrame(ctx.scope_stack->temp_arena, temp_frame);
    };

    auto pop_node = pushNode(ctx, node, res_type);

    auto const &token = src.tokens.data[node.token_idx];

    auto const prev_line = nkir_getLine(c->ir);
    defer {
        nkir_setLine(c->ir, prev_line);
    };
    nkir_setLine(c->ir, token.lin);

    NK_LOG_DBG("Compiling node %u #%s", nodeIdx(src, node), nk_atom2cs(node.id));

    auto node_it = nodeIterate(src, node);

    switch (node.id) {
        // TODO: Typecheck arithmetic
#define COMPILE_NUM(NAME, IR_NAME)                                                              \
    case NK_CAT(n_, NAME): {                                                                    \
        DEFINE(lhs, compile(ctx, nextNode(node_it), res_type));                                 \
        DEFINE(rhs, compile(ctx, nextNode(node_it), lhs.type));                                 \
        return {                                                                                \
            {.instr{NK_CAT(nkir_make_, IR_NAME)(c->ir, {}, asRef(ctx, lhs), asRef(ctx, rhs))}}, \
            lhs.type,                                                                           \
            IntermKind_Instr};                                                                  \
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

        // TODO: Typecheck comparisons
#define COMPILE_CMP(NAME)                                                                        \
    case NK_CAT(n_, NAME): {                                                                     \
        DEFINE(lhs, compile(ctx, nextNode(node_it)));                                            \
        DEFINE(rhs, compile(ctx, nextNode(node_it), lhs.type));                                  \
        return {                                                                                 \
            {.instr{NK_CAT(nkir_make_cmp_, NAME)(c->ir, {}, asRef(ctx, lhs), asRef(ctx, rhs))}}, \
            nkl_get_bool(nkl),                                                                   \
            IntermKind_Instr};                                                                   \
    }

        COMPILE_CMP(eq)
        COMPILE_CMP(ne)
        COMPILE_CMP(lt)
        COMPILE_CMP(le)
        COMPILE_CMP(gt)
        COMPILE_CMP(ge)

#undef COMPILE_CMP

#define COMPILE_TYPE(NAME, EXPR)                                                               \
    case NK_CAT(n_, NAME): {                                                                   \
        auto type_t = nkl_get_typeref(nkl, c->word_size);                                      \
        auto rodata = nkir_makeRodata(c->ir, 0, nklt2nkirt(type_t), NkIrVisibility_Local);     \
        *(nkltype_t *)nkir_getDataPtr(c->ir, rodata) = EXPR;                                   \
        return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, type_t, IntermKind_Val}; \
    }

        COMPILE_TYPE(void, (nkl_get_void(nkl)))

#define COMPILE_NUM_TYPE(NAME, VALUE_TYPE) COMPILE_TYPE(NAME, (nkl_get_numeric(nkl, VALUE_TYPE)))
        NKIR_NUMERIC_ITERATE(COMPILE_NUM_TYPE)
#undef COMPILE_NUM_TYPE

#undef COMPILE_TYPE

        case n_assign: {
            auto &lhs_n = nextNode(node_it);
            auto &rhs_n = nextNode(node_it);

            DEFINE(lhs, getLvalueRef(ctx, lhs_n));
            DEFINE(rhs, compile(ctx, rhs_n, lhs.type));

            return store(ctx, asRef(ctx, lhs), rhs);
        }

        case n_call: {
            auto &lhs_n = nextNode(node_it);
            auto &args_n = nextNode(node_it);

            DEFINE(lhs, compile(ctx, lhs_n));

            if (lhs.type->tclass != NklType_Procedure) {
                // TODO: Improve error message
                return error(ctx, "procedure expected");
            }

            auto temp_alloc = nk_arena_getAllocator(ctx.scope_stack->temp_arena);
            NkDynArray(Interm) args{NKDA_INIT(temp_alloc)};

            auto const param_types = lhs.type->ir_type.as.proc.info.args_t;

            auto args_it = nodeIterate(src, args_n);
            for (usize i = 0; i < args_n.arity; i++) {
                auto &arg_n = nextNode(args_it);
                auto const arg_t = i < param_types.size ? nkirt2nklt(param_types.data[i]) : nullptr;
                APPEND(&args, compile(ctx, arg_n, arg_t));
            }

            NkDynArray(NkIrRef) arg_refs{NKDA_INIT(temp_alloc)};
            for (usize i = 0; i < args.size; i++) {
                if (i == param_types.size) {
                    nkda_append(&arg_refs, nkir_makeVariadicMarkerRef(c->ir));
                }
                nkda_append(&arg_refs, asRef(ctx, args.data[i]));
            }

            return {
                {.instr{nkir_make_call(c->ir, {}, asRef(ctx, lhs), {NK_SLICE_INIT(arg_refs)})}},
                nkirt2nklt(lhs.type->ir_type.as.proc.info.ret_t),
                IntermKind_Instr};
        }

        case n_const: {
            auto &name_n = nextNode(node_it);
            auto &value_n = nextNode(node_it);

            auto name = nk_s2atom(nkl_getTokenStr(&src.tokens.data[name_n.token_idx], src.text));

            CHECK(defineComptimeUnresolved(ctx, name, value_n));

            return {};
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

            auto name_token = &src.tokens.data[name_n.token_idx];
            auto name_str = nkl_getTokenStr(name_token, src.text);
            auto name = nk_s2atom(name_str);

            NK_LOG_DBG("Resolving id: name=`%s` scope=%p", nk_atom2cs(name), (void *)scope);

            auto found = DeclMap_find(&scope->locals, name);
            if (found) {
                return resolveDecl(c, found->val);
            } else {
                return error(ctx, "`" NKS_FMT "` is not defined", NKS_ARG(name_str));
            }

            break;
        }

        case n_escaped_string: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            NkString const text{token_str.data + 1, token_str.size - 2};

            NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
            nks_unescape(nksb_getStream(&sb), text);
            NkString const unescaped_text{NKS_INIT(sb)};

            auto i8_t = nkl_get_numeric(nkl, Int8);
            auto ar_t = nkl_get_array(nkl, i8_t, unescaped_text.size + 1);
            auto str_t = nkl_get_ptr(nkl, c->word_size, ar_t, true);

            auto rodata = nkir_makeRodata(c->ir, 0, nklt2nkirt(ar_t), NkIrVisibility_Local);
            auto str_ptr = nkir_getDataPtr(c->ir, rodata);

            // TODO: Manual copy and null termination
            memcpy(str_ptr, unescaped_text.data, unescaped_text.size);
            ((char *)str_ptr)[unescaped_text.size] = '\0';

            return {
                {.ref = nkir_makeAddressRef(c->ir, nkir_makeDataRef(c->ir, rodata), nklt2nkirt(str_t))},
                str_t,
                IntermKind_Ref};
        }

        case n_extern_c_proc: {
            auto const decl_name = getConstDeclName(ctx);
            if (!decl_name) {
                // TODO: Improve error message
                return error(ctx, "decl name needed for extern c proc");
            }

            auto const proc_t = compileProcType(ctx, node, nullptr, NkCallConv_Cdecl);

            // TODO: Using hardcoded libc name
            auto const proc = nkir_makeExternProc(c->ir, nk_cs2atom(LIBC_NAME), decl_name, nklt2nkirt(proc_t));
            CHECK(defineExternProc(ctx, decl_name, proc));

            return {{.ref{nkir_makeExternProcRef(c->ir, proc)}}, proc_t, IntermKind_Ref};
        }

        case n_id: {
            auto token = &src.tokens.data[node.token_idx];
            auto name_str = nkl_getTokenStr(token, src.text);
            auto name = nk_s2atom(name_str);

            auto &decl = resolve(ctx, name);

            // TODO: Handle cross frame references

            if (decl.kind == DeclKind_Undefined) {
                return error(ctx, "`" NKS_FMT "` is not defined", NKS_ARG(name_str));
            } else {
                return resolveDecl(c, decl);
            }
        }

        case n_import: {
            auto &path_n = nextNode(node_it);
            auto path_str = nkl_getTokenStr(&src.tokens.data[path_n.token_idx], src.text);
            NkString const path{path_str.data + 1, path_str.size - 2};

            NKSB_FIXED_BUFFER(path_buf, NK_MAX_PATH);
            nksb_printf(&path_buf, NKS_FMT, NKS_ARG(path));
            nksb_appendNull(&path_buf);

            NkString import_path{};

            if (nk_pathIsRelative(path_buf.data)) {
                auto parent = nk_path_getParent(nk_atom2s(src.file));

                nksb_clear(&path_buf);
                nksb_printf(&path_buf, NKS_FMT "%c" NKS_FMT, NKS_ARG(parent), NK_PATH_SEPARATOR, NKS_ARG(path));

                import_path = {NKS_INIT(path_buf)};
            } else {
                import_path = path;
            }

            DEFINE(&imported_ctx, *importFile(c, import_path));

            return {
                {.val{
                    {.proc{.id = imported_ctx.top_level_proc, .opt_scope = imported_ctx.scope_stack}}, ValueKind_Proc}},
                nkirt2nklt(nkir_getProcType(c->ir, imported_ctx.top_level_proc)),
                IntermKind_Val};
        }

        case n_int: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            auto const i64_t = nkl_get_numeric(nkl, Int64);
            auto const rodata = nkir_makeRodata(c->ir, 0, nklt2nkirt(i64_t), NkIrVisibility_Local);
            // TODO: Replace sscanf in compiler
            int res = sscanf(token_str.data, "%" SCNi64, (i64 *)nkir_getDataPtr(c->ir, rodata));
            nk_assert(res > 0 && res != EOF && "numeric constant parsing failed");
            return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, i64_t, IntermKind_Val};
        }

        case n_float: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            auto const f64_t = nkl_get_numeric(nkl, Float64);
            auto const rodata = nkir_makeRodata(c->ir, 0, nklt2nkirt(f64_t), NkIrVisibility_Local);
            // TODO: Replace sscanf in compiler
            int res = sscanf(token_str.data, "%lf", (f64 *)nkir_getDataPtr(c->ir, rodata));
            nk_assert(res > 0 && res != EOF && "numeric constant parsing failed");
            return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, f64_t, IntermKind_Val};
        }

        case n_list: {
            for (usize i = 0; i < node.arity; i++) {
                CHECK(compileStmt(ctx, nextNode(node_it)));
            }
            break;
        }

        case n_addr: {
            DEFINE(arg, compile(ctx, nextNode(node_it)));
            return {
                {.instr{nkir_make_lea(c->ir, {}, asRef(ctx, arg))}},
                nkl_get_ptr(nkl, c->word_size, arg.type, false),
                IntermKind_Instr};
        }

        case n_deref:
        case n_member:
            return getLvalueRef(ctx, node);

        case n_proc: {
            auto const decl_name = getConstDeclName(ctx);

            nextNode(node_it); // params
            nextNode(node_it); // ret_t
            auto &body_n = nextNode(node_it);

            auto temp_alloc = nk_arena_getAllocator(ctx.scope_stack->temp_arena);

            NkAtomDynArray param_names{NKDA_INIT(temp_alloc)};
            auto const proc_t = compileProcType(ctx, node, &param_names);

            auto const proc_info = compileProc(
                ctx,
                NkIrProcDescr{
                    .name = decl_name, // TODO: Need to generate names for anonymous procs
                    .proc_t = nklt2nkirt(proc_t),
                    .arg_names{NK_SLICE_INIT(param_names)},
                    .file = ctx.src.file,
                    .line = token.lin,
                    .visibility = NkIrVisibility_Default, // TODO: Hardcoded visibility
                },
                body_n);

            return {{.val{{.proc{proc_info}}, ValueKind_Proc}}, proc_t, IntermKind_Val};
        }

        case n_proc_type: {
            DEFINE(const proc_t, compileProcType(ctx, node));

            auto type_t = nkl_get_typeref(nkl, c->word_size);

            auto rodata = nkir_makeRodata(c->ir, 0, nklt2nkirt(type_t), NkIrVisibility_Local);
            *(nkltype_t *)nkir_getDataPtr(c->ir, rodata) = proc_t;

            return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, type_t, IntermKind_Val};
        }

        case n_ptr: {
            auto &target_n = nextNode(node_it);

            DEFINE(target, compile(ctx, target_n, nkl_get_typeref(nkl, c->word_size)));

            // TODO: Boilerplate in type checking
            if (target.type->tclass != NklType_Typeref) {
                // TODO: Improve error message
                return error(ctx, "type expected");
            }

            if (!isValueKnown(target)) {
                // TODO: Improve error message
                return error(ctx, "value is not known");
            }

            auto type_t = nkl_get_typeref(nkl, c->word_size);

            auto rodata = nkir_makeRodata(c->ir, 0, nklt2nkirt(type_t), NkIrVisibility_Local);
            auto target_t = nklval_as(nkltype_t, getValueFromInterm(c, target));
            *(nkltype_t *)nkir_getDataPtr(c->ir, rodata) = nkl_get_ptr(nkl, c->word_size, target_t, false);

            return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, type_t, IntermKind_Val};
        }

        case n_return: {
            // TODO: Typecheck the returned value
            if (node.arity) {
                auto &arg_n = nextNode(node_it);
                DEFINE(arg, compile(ctx, arg_n)); // TODO: Get the current proc return type
                nkir_emit(c->ir, nkir_make_mov(c->ir, nkir_makeRetRef(c->ir), asRef(ctx, arg)));
            }
            nkir_emit(c->ir, nkir_make_ret(c->ir));
            return {{}, nkl_get_void(nkl), IntermKind_Void};
        }

        case n_run: {
            auto &ret_t_n = nextNode(node_it);
            auto &body_n = nextNode(node_it);

            nkltype_t ret_t{};
            if (ret_t_n.id) {
                DEFINE(ret_t_v, compile(ctx, ret_t_n, nkl_get_typeref(nkl, c->word_size)));
                // TODO: Boilerplate in type checking
                if (ret_t_v.type->tclass != NklType_Typeref) {
                    // TODO: Improve error message
                    return error(ctx, "type expected");
                }

                if (!isValueKnown(ret_t_v)) {
                    // TODO: Improve error message
                    return error(ctx, "value is not known");
                }

                ret_t = nklval_as(nkltype_t, getValueFromInterm(c, ret_t_v));
            } else {
                ret_t = nkl_get_void(nkl);
            }

            auto proc_t = nkl_get_proc(
                nkl,
                c->word_size,
                NklProcInfo{
                    .param_types{},
                    .ret_t = ret_t,
                    .call_conv = NkCallConv_Nk,
                    .flags{},
                });

            auto const proc_info = compileProc(
                ctx,
                NkIrProcDescr{
                    .name = nk_cs2atom("__comptime"), // TODO: Hardcoded comptime proc name
                    .proc_t = nklt2nkirt(proc_t),
                    .arg_names{},
                    .file = ctx.src.file,
                    .line = token.lin,
                    .visibility = NkIrVisibility_Local,
                },
                body_n);

            void *rets[] = {nullptr};
            NkIrData rodata{};

            if (nklt_sizeof(ret_t)) {
                rodata = nkir_makeRodata(c->ir, 0, nklt2nkirt(ret_t), NkIrVisibility_Local);
                rets[0] = {nkir_getDataPtr(c->ir, rodata)};
            }

            if (!nkir_invoke(c->run_ctx, proc_info.id, {}, rets)) {
                auto const msg = nkir_getRunErrorString(c->run_ctx);
                nkl_reportError(ctx.src.file, &token, NKS_FMT, NKS_ARG(msg));
                return {};
            }

            if (nklt_sizeof(ret_t)) {
                return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, ret_t, IntermKind_Val};
            } else {
                return {{}, nkl_get_void(nkl), IntermKind_Void};
            }
        }

        case n_scope: {
            pushPrivateScope(ctx, ctx.scope_stack->cur_proc);
            defer {
                popScope(ctx);
            };

            auto &child_n = nextNode(node_it);
            CHECK(compileStmt(ctx, child_n));

            return {};
        }

            // TODO: Boilerplate code between n_escaped_string and n_string
        case n_string: {
            auto const token_str = nkl_getTokenStr(&src.tokens.data[node.token_idx], src.text);
            NkString const text{token_str.data + 1, token_str.size - 2};

            auto i8_t = nkl_get_numeric(nkl, Int8);
            auto ar_t = nkl_get_array(nkl, i8_t, text.size + 1);
            auto str_t = nkl_get_ptr(nkl, c->word_size, ar_t, true);

            auto rodata = nkir_makeRodata(c->ir, 0, nklt2nkirt(ar_t), NkIrVisibility_Local);
            auto str_ptr = nkir_getDataPtr(c->ir, rodata);

            // TODO: Manual copy and null termination
            memcpy(str_ptr, text.data, text.size);
            ((char *)str_ptr)[text.size] = '\0';

            return {
                {.ref{nkir_makeAddressRef(c->ir, nkir_makeDataRef(c->ir, rodata), nklt2nkirt(str_t))}},
                str_t,
                IntermKind_Ref};
        }

        case n_struct: {
            auto &params_n = nextNode(node_it);

            auto temp_alloc = nk_arena_getAllocator(ctx.scope_stack->temp_arena);

            NkDynArray(NklField) fields{NKDA_INIT(temp_alloc)};

            // TODO: Boilerplate in param compilation
            auto params_it = nodeIterate(src, params_n);
            for (usize i = 0; i < params_n.arity; i++) {
                auto param_it = nodeIterate(src, nextNode(params_it));

                auto &name_n = nextNode(param_it);
                auto &type_n = nextNode(param_it);

                auto name_token = &src.tokens.data[name_n.token_idx];
                auto name_str = nkl_getTokenStr(name_token, src.text);
                auto name = nk_s2atom(name_str);

                DEFINE(type_v, compile(ctx, type_n, nkl_get_typeref(nkl, c->word_size)));

                if (type_v.type->tclass != NklType_Typeref) {
                    // TODO: Improve error message
                    return error(ctx, "type expected");
                }

                if (!isValueKnown(type_v)) {
                    // TODO: Improve error message
                    return error(ctx, "value is not known");
                }

                auto type = nklval_as(nkltype_t, getValueFromInterm(c, type_v));

                nkda_append(&fields, NklField{name, type});
            }

            auto struct_t = nkl_get_struct(nkl, {NK_SLICE_INIT(fields)});

            auto type_t = nkl_get_typeref(nkl, c->word_size);

            auto rodata = nkir_makeRodata(c->ir, 0, nklt2nkirt(type_t), NkIrVisibility_Local);
            *(nkltype_t *)nkir_getDataPtr(c->ir, rodata) = struct_t;

            return {{.val{{.rodata{rodata, nullptr}}, ValueKind_Rodata}}, type_t, IntermKind_Val};
        }

        case n_var: {
            auto &name_n = nextNode(node_it);
            auto &type_n = nextNode(node_it);
            auto &val_n = nextNode(node_it);

            auto name = nk_s2atom(nkl_getTokenStr(&src.tokens.data[name_n.token_idx], src.text));

            if (!type_n.id && !val_n.id) {
                // TODO: Improve error message
                return error(ctx, "invalid ast");
            }

            nkltype_t type{};

            if (type_n.id) {
                DEFINE(type_v, compile(ctx, type_n, nkl_get_typeref(nkl, c->word_size)));

                if (type_v.type->tclass != NklType_Typeref) {
                    // TODO: Improve error message
                    return error(ctx, "type expected");
                }

                if (!isValueKnown(type_v)) {
                    // TODO: Improve error message
                    return error(ctx, "value is not known");
                }

                type = nklval_as(nkltype_t, getValueFromInterm(c, type_v));
            }

            Interm val{};
            if (val_n.id) {
                ASSIGN(val, compile(ctx, val_n, type));

                if (type_n.id) {
                    // TODO: Typecheck in n_var
                } else {
                    type = val.type;
                }
            }

            auto var = nkir_makeLocalVar(c->ir, name, nklt2nkirt(type));
            CHECK(defineLocal(ctx, name, var));

            if (val_n.id) {
                CHECK(store(ctx, nkir_makeFrameRef(c->ir, var), val));
            }

            return {{}, nkl_get_void(nkl), IntermKind_Void};
        }

        case n_if: {
            auto &cond_n = nextNode(node_it);
            auto &body_n = nextNode(node_it);
            auto &else_n = nextNode(node_it);

            auto const endif_l = nkir_createLabel(c->ir, nk_cs2atom("@endif"));
            NkIrLabel else_l;
            if (else_n.id) {
                else_l = nkir_createLabel(c->ir, nk_cs2atom("@else"));
            } else {
                else_l = endif_l;
            }

            DEFINE(cond, compile(ctx, cond_n, nkl_get_bool(nkl)));

            if (cond.type->tclass != NklType_Bool) {
                // TODO: Improve error message
                return error(ctx, "bool expected");
            }

            nkir_emit(c->ir, nkir_make_jmpz(c->ir, asRef(ctx, cond), else_l));

            {
                pushPrivateScope(ctx, ctx.scope_stack->cur_proc);
                defer {
                    popScope(ctx);
                };
                CHECK(compileStmt(ctx, body_n));
            }

            if (else_n.id) {
                nkir_emit(c->ir, nkir_make_label(else_l));
                {
                    pushPrivateScope(ctx, ctx.scope_stack->cur_proc);
                    defer {
                        popScope(ctx);
                    };
                    CHECK(compileStmt(ctx, else_n));
                }
            }

            nkir_emit(c->ir, nkir_make_label(endif_l));

            return {{}, nkl_get_void(nkl), IntermKind_Void};
        }

        case n_while: {
            auto &cond_n = nextNode(node_it);
            auto &body_n = nextNode(node_it);

            auto const loop_l = nkir_createLabel(c->ir, nk_cs2atom("@loop"));
            auto const endloop_l = nkir_createLabel(c->ir, nk_cs2atom("@endloop"));

            nkir_emit(c->ir, nkir_make_label(loop_l));

            DEFINE(cond, compile(ctx, cond_n, nkl_get_bool(nkl)));

            if (cond.type->tclass != NklType_Bool) {
                // TODO: Improve error message
                return error(ctx, "bool expected");
            }

            nkir_emit(c->ir, nkir_make_jmpz(c->ir, asRef(ctx, cond), endloop_l));

            {
                pushPrivateScope(ctx, ctx.scope_stack->cur_proc);
                defer {
                    popScope(ctx);
                };
                CHECK(compileStmt(ctx, body_n));
            }

            nkir_emit(c->ir, nkir_make_jmp(c->ir, loop_l));
            nkir_emit(c->ir, nkir_make_label(endloop_l));

            return {{}, nkl_get_void(nkl), IntermKind_Void};
        }

        default: {
            return error(ctx, "unknown AST node #%s", nk_atom2cs(node.id));
        }
    }

    return {};
}

static Void compileStmt(Context &ctx, NklAstNode const &node) {
    auto m = ctx.m;
    auto c = m->c;

    DEFINE(val, compile(ctx, node));
    auto const ref = asRef(ctx, val);
    if (ref.kind != NkIrRef_None && ref.type->id != nkl_get_void(ctx.m->c->nkl)->id) {
        NKSB_FIXED_BUFFER(sb, 1024);
        nkir_inspectRef(c->ir, ctx.scope_stack->cur_proc, ref, nksb_getStream(&sb));
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

    // TODO: Storing entry point for the whole compiler on every compileFile call
    c->entry_point = ctx.top_level_proc;

    nkir_mergeModules(m->mod, ctx.m->mod);

    // TODO: Inspect module?

#ifdef ENABLE_LOGGING
    {
        NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
        nkir_inspectExternSyms(c->ir, nksb_getStream(&sb));
        NK_LOG_INF("IR:\n" NKS_FMT, NKS_ARG(sb));
    }
#endif // ENABLE_LOGGING

    return true;
}
