#include "compiler_state.hpp"

#include "nkb/ir.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/list.h"
#include "ntk/log.h"
#include "ntk/slice.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"

namespace {

NK_LOG_USE_SCOPE(compiler);

static u64 nk_atom_hash(NkAtom const key) {
    return nk_hashVal(key);
}
static bool nk_atom_equal(NkAtom const lhs, NkAtom const rhs) {
    return lhs == rhs;
}

static NkAtom const *Decl_kv_GetKey(Decl_kv const *item) {
    return &item->key;
}

static NkAtom const *FileContext_kv_GetKey(FileContext_kv const *item) {
    return &item->key;
}

static NkAtom const *NkAtom_GetKey(NkAtom const *item) {
    return item;
}

} // namespace

NK_HASH_TREE_IMPL(DeclMap, Decl_kv, NkAtom, Decl_kv_GetKey, nk_atom_hash, nk_atom_equal);
NK_HASH_TREE_IMPL(FileContextMap, FileContext_kv, NkAtom, FileContext_kv_GetKey, nk_atom_hash, nk_atom_equal);
NK_HASH_TREE_IMPL(NkAtomSet, NkAtom, NkAtom, NkAtom_GetKey, nk_atom_hash, nk_atom_equal);

NkArena *getNextTempArena(NklCompiler c, NkArena *conflict) {
    return &c->temp_arenas[0] == conflict ? &c->temp_arenas[1] : &c->temp_arenas[0];
}

FileContext_kv &getContextForFile(NklCompiler c, NkAtom file) {
    auto found = FileContextMap_find(&c->files, file);
    if (!found) {
        found = FileContextMap_insert(&c->files, {file, {}});
    }
    return *found;
}

NkIrLabel createLabel(Context &ctx, LabelName name) {
    char const *fmt = "<nil>";
    switch (name) {
        case LabelName_Start:
            fmt = "@start%u";
            break;
        case LabelName_Else:
            fmt = "@else%u";
            break;
        case LabelName_Endif:
            fmt = "@endif%u";
            break;
        case LabelName_Short:
            fmt = "@short%u";
            break;
        case LabelName_Join:
            fmt = "@join%u";
            break;
        case LabelName_Loop:
            fmt = "@loop%u";
            break;
        case LabelName_Endloop:
            fmt = "@endloop%u";
            break;

        case LabelName_Count:
            nk_assert(!"unreachable");
    }
    auto const count = ctx.proc_stack->label_counts[name]++;
    return nkir_createLabel(ctx.ir, nk_s2atom(nk_tsprintf(ctx.scope_stack->temp_arena, fmt, count)));
}

void emit(Context &ctx, NkIrInstr const &instr) {
    nk_assert(ctx.proc_stack && "no current proc");

    if (instr.code == nkir_label) {
        ctx.proc_stack->has_return_in_last_block = false;
    } else if (instr.code == nkir_ret) {
        ctx.proc_stack->has_return_in_last_block = true;
    }

#ifdef ENABLE_LOGGING
    NkStringBuilder sb{NKSB_INIT(nk_arena_getAllocator(ctx.scope_stack->temp_arena))};
    nkir_inspectInstr(ctx.ir, ctx.proc_stack->proc, instr, nksb_getStream(&sb));
#endif // ENABLE_LOGGING

    if (ctx.proc_stack->active_defer_node) {
        auto const defer_node = ctx.proc_stack->active_defer_node;

        NK_LOG_DBG(
            "Emitting '" NKS_FMT "' into a defer node#%u file=`%s`",
            NKS_ARG(sb),
            defer_node->node_idx,
            nk_atom2cs(defer_node->file));

        nkda_append(&defer_node->instrs, instr);
    } else {
        NK_LOG_DBG("Emitting '" NKS_FMT "'", NKS_ARG(sb));

        nkir_emit(ctx.ir, instr);
    }
}

static void emitDefersForScope(Context &ctx, Scope const *scope) {
    auto defer_node = scope->defer_stack;
    while (defer_node) {
        NK_LOG_DBG("Emitting defer blocks for node#%u file=`%s`", defer_node->node_idx, nk_atom2cs(defer_node->file));

        if (ctx.proc_stack->active_defer_node) {
            nkir_instrArrayDupInto(
                ctx.ir, {NK_SLICE_INIT(defer_node->instrs)}, &ctx.proc_stack->active_defer_node->instrs, scope->temp_arena);
        } else {
            auto frame = nk_arena_grab(scope->temp_arena);
            defer {
                nk_arena_popFrame(scope->temp_arena, frame);
            };
            NkIrInstrDynArray instrs_copy{NKDA_INIT(nk_arena_getAllocator(scope->temp_arena))};
            nkir_instrArrayDupInto(ctx.ir, {NK_SLICE_INIT(defer_node->instrs)}, &instrs_copy, scope->temp_arena);
            nkir_emitArray(ctx.ir, {NK_SLICE_INIT(instrs_copy)});
        }

        defer_node = defer_node->next;
    }
}

void emitDefers(Context &ctx, Scope const *upto) {
    if (ctx.proc_stack->has_return_in_last_block) {
        return;
    }

    auto scope = ctx.scope_stack;

    while (scope != upto) {
        emitDefersForScope(ctx, scope);
        scope = scope->next;
    }
}

NkIrRef asRef(Context &ctx, Interm const &val) {
    NkIrRef ref{};

    switch (val.kind) {
        case IntermKind_Void:
            break;

        case IntermKind_Instr: {
            auto instr = val.as.instr;
            auto &dst = instr.arg[0].ref;
            if (dst.kind == NkIrRef_None && nklt_sizeof(val.type)) {
                dst = nkir_makeFrameRef(ctx.ir, nkir_makeLocalVar(ctx.ir, 0, nklt2nkirt(val.type)));
            }
            emit(ctx, instr);
            ref = dst;
            break;
        }

        case IntermKind_Ref:
            ref = val.as.ref;
            break;

        case IntermKind_Val:
            switch (val.as.val.kind) {
                case ValueKind_Void:
                    ref = {};
                    break;

                case ValueKind_Rodata:
                case ValueKind_Data:
                    ref = nkir_makeDataRef(ctx.ir, val.as.val.as.data.id);
                    break;
                case ValueKind_Proc:
                    ref = nkir_makeProcRef(ctx.ir, val.as.val.as.proc.id);
                    break;
                case ValueKind_ExternData:
                    ref = nkir_makeExternDataRef(ctx.ir, val.as.val.as.extern_data.id);
                    break;
                case ValueKind_ExternProc:
                    ref = nkir_makeExternProcRef(ctx.ir, val.as.val.as.extern_proc.id);
                    break;
                case ValueKind_Local:
                    ref = nkir_makeFrameRef(ctx.ir, val.as.val.as.local.id);
                    break;
                case ValueKind_Arg:
                    ref = nkir_makeArgRef(ctx.ir, val.as.val.as.arg.idx);
                    break;
            }
            break;
    }

    return ref;
}

static void pushScope(Context &ctx, NkArena *main_arena, NkArena *temp_arena) {
    nk_assert(ctx.proc_stack && "no current proc");

    auto const alloc = nk_arena_getAllocator(main_arena);
    auto scope = new (nk_allocT<Scope>(alloc)) Scope{
        .next{},

        .main_arena = main_arena,
        .temp_arena = temp_arena,
        .temp_frame = nk_arena_grab(temp_arena),

        .locals{nullptr, alloc},

        .export_list{},
        .defer_stack{},

        .cur_proc = ctx.proc_stack,
    };
    NK_LOG_DBG("Entering scope=%p", (void *)scope);
    nk_list_push(ctx.scope_stack, scope);
}

void pushPublicScope(Context &ctx) {
    auto cur_scope = ctx.scope_stack;
    if (cur_scope) {
        pushScope(ctx, cur_scope->main_arena, cur_scope->temp_arena);
    } else {
        pushScope(ctx, &ctx.c->perm_arena, getNextTempArena(ctx.c, nullptr));
    }
}

void pushPrivateScope(Context &ctx) {
    auto cur_scope = ctx.scope_stack;
    NK_LOG_DBG("Leaving scope=%p", (void *)cur_scope);
    nk_assert(cur_scope && "top level scope cannot be private");
    pushScope(ctx, cur_scope->temp_arena, getNextTempArena(ctx.c, cur_scope->temp_arena));
}

void popScope(Context &ctx) {
    nk_assert(ctx.scope_stack && "no current scope");
    nk_arena_popFrame(ctx.scope_stack->temp_arena, ctx.scope_stack->temp_frame);
    nk_list_pop(ctx.scope_stack);
}

static Decl &makeDecl(Context &ctx, NkAtom name) {
    nk_assert(ctx.scope_stack && "no current scope");
    NK_LOG_DBG("Declaring name=`%s` scope=%p", nk_atom2cs(name), (void *)ctx.scope_stack);
    auto const found = DeclMap_find(&ctx.scope_stack->locals, name);
    if (found) {
        static Decl s_dummy{};
        return error(ctx, "redefinition of '%s'", nk_atom2cs(name)), s_dummy;
    }
    auto kv = DeclMap_insert(&ctx.scope_stack->locals, {name, {}});
    return kv->val;
}

void defineComptimeUnresolved(Context &ctx, NkAtom name, NklAstNode const &node) {
    // TODO: Choose arena based on the symbol visibility
    auto ctx_copy = new (nk_arena_allocT<Context>(ctx.scope_stack->main_arena)) Context{
        .nkl = ctx.nkl,
        .c = ctx.c,
        .m = ctx.m,
        .ir = ctx.ir,

        .top_level_proc = ctx.top_level_proc,
        .src = ctx.src,

        .scope_stack = ctx.scope_stack,
        .node_stack = ctx.node_stack,
        .proc_stack = ctx.proc_stack,
    };
    makeDecl(ctx, name) = {{.unresolved{ctx_copy, &node}}, DeclKind_Unresolved};
}

void defineLocal(Context &ctx, NkAtom name, NkIrLocalVar var) {
    makeDecl(ctx, name) = {{.val{{.local{var}}, ValueKind_Local}}, DeclKind_Complete};
}

void defineParam(Context &ctx, NkAtom name, usize idx) {
    makeDecl(ctx, name) = {{.val{{.arg{idx}}, ValueKind_Arg}}, DeclKind_Complete};
}

Decl &resolve(Context &ctx, NkAtom name) {
    auto scope = ctx.scope_stack;

    NK_LOG_DBG("Resolving id: name=`%s` scope=%p", nk_atom2cs(name), (void *)scope);

    for (; scope; scope = scope->next) {
        auto found = DeclMap_find(&scope->locals, name);
        if (found) {
            return found->val;
        }
    }

    static Decl s_undefined_decl{{}, DeclKind_Undefined};
    return s_undefined_decl;
}

bool isValueKnown(Interm const &val) {
    return val.kind == IntermKind_Void ||
           (val.kind == IntermKind_Val && (val.as.val.kind == ValueKind_Proc || val.as.val.kind == ValueKind_Rodata ||
                                           val.as.val.kind == ValueKind_ExternProc));
}

nklval_t getValueFromInterm(Context &ctx, Interm const &val) {
    nk_assert(isValueKnown(val) && "trying to get an unknown value");

    switch (val.kind) {
        case IntermKind_Void:
            return {nullptr, val.type};
        case IntermKind_Val:
            switch (val.as.val.kind) {
                case ValueKind_Rodata:
                    return {nkir_getDataPtr(ctx.ir, val.as.val.as.rodata.id), val.type};
                case ValueKind_Proc:
                    nk_assert(!"getValueFromInfo is not implemented for ValueKind_Proc");
                    return {};

                    // TODO: Can we consider extern proc a known value?
                case ValueKind_ExternProc:
                    nk_assert(!"trying to get a comptime value from an extern proc const");
                    return {};

                case ValueKind_Void:
                case ValueKind_Data:
                case ValueKind_ExternData:
                case ValueKind_Local:
                case ValueKind_Arg:
                    nk_assert(!"unreachable");
                    return {};
            }
            return {};

        case IntermKind_Ref:
        case IntermKind_Instr:
            nk_assert(!"unreachable");
            return {};
    }

    nk_assert(!"unreachable");
    return {};
}

NkIrData getRodataFromInterm(Context &ctx, Interm const &val) {
    nk_assert(isValueKnown(val) && "trying to get an unknown value");
    auto ref = asRef(ctx, val);
    nk_assert(ref.kind == NkIrRef_Data && nkir_dataIsReadOnly(ctx.ir, {ref.index}));
    return {ref.index};
}

bool isModule(Interm const &val) {
    return val.kind == IntermKind_Val && ((val.as.val.kind == ValueKind_Rodata && val.as.val.as.rodata.opt_scope) ||
                                          (val.as.val.kind == ValueKind_Proc && val.as.val.as.proc.opt_scope));
}

Scope *getModuleScope(Interm const &val) {
    nk_assert(isModule(val) && "module expected");
    switch (val.as.val.kind) {
        case ValueKind_Rodata:
            return val.as.val.as.rodata.opt_scope;
        case ValueKind_Proc:
            return val.as.val.as.proc.opt_scope;

        case ValueKind_Void:
        case ValueKind_Data:
        case ValueKind_ExternData:
        case ValueKind_ExternProc:
        case ValueKind_Local:
        case ValueKind_Arg:
            nk_assert(!"unreachable");
            return {};
    }

    nk_assert(!"unreachable");
    return {};
}

nkltype_t getValueType(Context &ctx, Value const &val) {
    switch (val.kind) {
        case ValueKind_Void:
            return nkl_get_void(ctx.nkl);
        case ValueKind_Rodata:
            return nkirt2nklt(nkir_getDataType(ctx.ir, val.as.rodata.id));
        case ValueKind_Proc:
            return nkirt2nklt(nkir_getProcType(ctx.ir, val.as.proc.id));
        case ValueKind_Data:
            return nkirt2nklt(nkir_getDataType(ctx.ir, val.as.data.id));
        case ValueKind_ExternData:
            return nkirt2nklt(nkir_getExternDataType(ctx.ir, val.as.extern_data.id));
        case ValueKind_ExternProc:
            return nkirt2nklt(nkir_getExternProcType(ctx.ir, val.as.extern_proc.id));
        case ValueKind_Local:
            return nkirt2nklt(nkir_getLocalType(ctx.ir, val.as.local.id));
        case ValueKind_Arg:
            return nkirt2nklt(nkir_getArgType(ctx.ir, val.as.arg.idx));
    }

    nk_assert(!"unreachable");
    return {};
}
