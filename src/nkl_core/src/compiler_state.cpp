#include "compiler_state.hpp"

#include "ntk/common.h"
#include "ntk/list.h"
#include "ntk/log.h"

namespace {

NK_LOG_USE_SCOPE(compiler);

} // namespace

NkArena *getNextTempArena(NklCompiler c, NkArena *conflict) {
    return &c->temp_arenas[0] == conflict ? &c->temp_arenas[1] : &c->temp_arenas[0];
}

FileContext &getContextForFile(NklCompiler c, NkAtom file) {
    auto found = CompilerFileMap_find(&c->files, file);
    if (!found) {
        found = CompilerFileMap_insert(&c->files, {file, {}});
    }
    return found->val;
}

NkIrRef asRef(Context &ctx, ValueInfo const &val) {
    NkIrRef ref{};

    auto m = ctx.m;
    auto c = m->c;

    switch (val.kind) {
        case ValueKind_Void:
            break;

        case ValueKind_Const:
            ref = nkir_makeDataRef(c->ir, val.as.cnst.id);
            break;

        case ValueKind_Decl: {
            auto &decl = *val.as.decl;
            switch (decl.kind) {
                case DeclKind_Comptime:
                    // TODO: asRef(DeclKind_Comptime)
                    nk_assert(!"asRef(DeclKind_Comptime) is not implemented");
                    break;
                case DeclKind_ComptimeIncomplete:
                    // TODO: asRef(DeclKind_ComptimeIncomplete)
                    nk_assert(!"asRef(DeclKind_ComptimeIncomplete) is not implemented");
                    break;
                case DeclKind_ComptimeUnresolved:
                    // TODO: asRef(DeclKind_ComptimeUnresolved)
                    nk_assert(!"asRef(DeclKind_ComptimeUnresolved) is not implemented");
                    break;
                case DeclKind_ExternData:
                    ref = nkir_makeExternDataRef(c->ir, decl.as.extern_data.id);
                    break;
                case DeclKind_ExternProc:
                    ref = nkir_makeExternProcRef(c->ir, decl.as.extern_proc.id);
                    break;
                case DeclKind_Local:
                    ref = nkir_makeFrameRef(c->ir, decl.as.local.var);
                    break;
                case DeclKind_Module:
                case DeclKind_ModuleIncomplete:
                    ref = nkir_makeProcRef(c->ir, decl.as.module.proc);
                    break;
                case DeclKind_Param:
                    ref = nkir_makeArgRef(c->ir, decl.as.param.idx);
                    break;
                case DeclKind_Undefined:
                    nk_assert(!"referencing an undefined declaration");
                default:
                    nk_assert(!"unreachable");
                    break;
            }
            break;
        }

        case ValueKind_Instr: {
            auto instr = val.as.instr;
            auto &dst = instr.arg[0].ref;
            if (dst.kind == NkIrRef_None && nklt_sizeof(val.type)) {
                dst = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, NK_ATOM_INVALID, nklt2nkirt(val.type)));
            }
            nkir_emit(c->ir, instr);
            ref = dst;
            break;
        }

        case ValueKind_Ref:
            ref = val.as.ref;
            break;

        default:
            nk_assert(!"unreachable");
            return {};
    }

    return ref;
}

usize parentNodeIdx(Context &ctx) {
    return ctx.node_stack->next ? ctx.node_stack->next->node_idx : -1u;
}

static void pushScope(Context &ctx, NkArena *main_arena, NkArena *temp_arena, NkIrProc cur_proc) {
    auto scope = new (nk_arena_allocT<Scope>(main_arena)) Scope{
        .next{},

        .main_arena = main_arena,
        .temp_arena = temp_arena,
        .temp_frame = nk_arena_grab(temp_arena),

        .locals{nullptr, nk_arena_getAllocator(main_arena)},

        .cur_proc = cur_proc,
    };
    nk_list_push(ctx.scope_stack, scope);
}

void pushPublicScope(Context &ctx, NkIrProc cur_proc) {
    auto cur_scope = ctx.scope_stack;
    if (cur_scope) {
        pushScope(ctx, cur_scope->main_arena, cur_scope->temp_arena, cur_proc);
    } else {
        auto m = ctx.m;
        auto c = m->c;
        pushScope(ctx, &c->perm_arena, getNextTempArena(c, nullptr), cur_proc);
    }
}

void pushPrivateScope(Context &ctx, NkIrProc cur_proc) {
    auto cur_scope = ctx.scope_stack;
    nk_assert(cur_scope && "top level scope cannot be private");
    auto m = ctx.m;
    auto c = m->c;
    pushScope(ctx, cur_scope->temp_arena, getNextTempArena(c, cur_scope->temp_arena), cur_proc);
}

void popScope(Context &ctx) {
    nk_assert(ctx.scope_stack && "no current scope");
    nk_arena_popFrame(ctx.scope_stack->temp_arena, ctx.scope_stack->temp_frame);
    nk_list_pop(ctx.scope_stack);
}

static Decl &makeDecl(Context &ctx, NkAtom name) {
    nk_assert(ctx.scope_stack && "no current scope");
    NK_LOG_DBG("Declaring name=`%s` scope=%p", nk_atom2cs(name), (void *)ctx.scope_stack);
    // TODO: Check for name conflict
    auto kv = DeclMap_insert(&ctx.scope_stack->locals, {name, {}});
    return kv->val;
}

void defineComptime(Context &ctx, NkAtom name, nklval_t val) {
    makeDecl(ctx, name) = {{.comptime{.val = val}}, DeclKind_Comptime};
}

void defineComptimeUnresolved(Context &ctx, NkAtom name, usize node_idx) {
    // TODO: Why do we need to copy the context??
    auto ctx_copy = nk_arena_allocT<Context>(ctx.scope_stack->main_arena);
    *ctx_copy = ctx;
    makeDecl(ctx, name) = {{.comptime_unresolved{.ctx = ctx_copy, .node_idx = node_idx}}, DeclKind_ComptimeUnresolved};
}

void defineLocal(Context &ctx, NkAtom name, NkIrLocalVar var) {
    makeDecl(ctx, name) = {{.local{.var = var}}, DeclKind_Local};
}

void defineParam(Context &ctx, NkAtom name, usize idx) {
    makeDecl(ctx, name) = {{.param{.idx = idx}}, DeclKind_Param};
}

void defineExternProc(Context &ctx, NkAtom name, NkIrExternProc id) {
    makeDecl(ctx, name) = {{.extern_proc{.id = id}}, DeclKind_ExternProc};
}

void defineExternData(Context &ctx, NkAtom name, NkIrExternData id) {
    makeDecl(ctx, name) = {{.extern_data{.id = id}}, DeclKind_ExternData};
}

Decl &resolve(Scope *scope, NkAtom name) {
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

bool isValueKnown(ValueInfo const &val) {
    return val.kind == ValueKind_Void || val.kind == ValueKind_Const ||
           (val.kind == ValueKind_Decl && (val.as.decl->kind == DeclKind_Comptime));
}

nklval_t getValueFromInfo(NklCompiler c, ValueInfo const &val) {
    nk_assert(isValueKnown(val) && "trying to get an unknown value");

    switch (val.kind) {
        case ValueKind_Void:
            return {nullptr, val.type};
        case ValueKind_Const:
            return {nkir_getDataPtr(c->ir, val.as.cnst.id), val.type};
        case ValueKind_Decl: {
            switch (val.as.decl->kind) {
                case DeclKind_Comptime:
                    return val.as.decl->as.comptime.val;
                default:
                    nk_assert(!"unreachable");
                    return {};
            }
        }
        default:
            nk_assert(!"unreachable");
            return {};
    }
}

bool isModule(ValueInfo const &val) {
    return val.kind == ValueKind_Decl &&
           (val.as.decl->kind == DeclKind_Module || val.as.decl->kind == DeclKind_ModuleIncomplete);
}

Scope *getModuleScope(ValueInfo const &val) {
    nk_assert(isModule(val) && "module expected");
    return val.as.decl->as.module.scope;
}
