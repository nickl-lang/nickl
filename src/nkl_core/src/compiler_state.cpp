#include "compiler_state.hpp"

#include "nkb/ir.h"
#include "ntk/common.h"
#include "ntk/list.h"
#include "ntk/log.h"

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

} // namespace

NK_HASH_TREE_IMPL(DeclMap, Decl_kv, NkAtom, Decl_kv_GetKey, nk_atom_hash, nk_atom_equal);
NK_HASH_TREE_IMPL(FileContextMap, FileContext_kv, NkAtom, FileContext_kv_GetKey, nk_atom_hash, nk_atom_equal);

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

NkIrRef asRef(Context &ctx, Interm const &val) {
    NkIrRef ref{};

    auto m = ctx.m;
    auto c = m->c;

    switch (val.kind) {
        case IntermKind_Void:
            break;

        case IntermKind_Instr: {
            auto instr = val.as.instr;
            auto &dst = instr.arg[0].ref;
            if (dst.kind == NkIrRef_None && nklt_sizeof(val.type)) {
                dst = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, NK_ATOM_INVALID, nklt2nkirt(val.type)));
            }
            nkir_emit(c->ir, instr);
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
                    ref = nkir_makeDataRef(c->ir, val.as.val.as.data.id);
                    break;
                case ValueKind_Proc:
                    ref = nkir_makeProcRef(c->ir, val.as.val.as.proc.id);
                    break;
                case ValueKind_ExternData:
                    ref = nkir_makeExternDataRef(c->ir, val.as.val.as.extern_data.id);
                    break;
                case ValueKind_ExternProc:
                    ref = nkir_makeExternProcRef(c->ir, val.as.val.as.extern_proc.id);
                    break;
                case ValueKind_Local:
                    ref = nkir_makeFrameRef(c->ir, val.as.val.as.local.id);
                    break;
                case ValueKind_Arg:
                    ref = nkir_makeArgRef(c->ir, val.as.val.as.arg.idx);
                    break;
            }
            break;
    }

    return ref;
}

NklAstNode const *parentNodePtr(Context &ctx) {
    return ctx.node_stack->next ? &ctx.node_stack->next->node : nullptr;
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

void defineComptimeUnresolved(Context &ctx, NkAtom name, NklAstNode const &node) {
    auto ctx_copy = new (nk_arena_allocT<Context>(ctx.scope_stack->main_arena)) Context{
        .top_level_proc = ctx.top_level_proc,
        .m = ctx.m,
        .src = ctx.src,
        .scope_stack = ctx.scope_stack,
        .node_stack = ctx.node_stack,
    };
    makeDecl(ctx, name) = {{.unresolved{.ctx = ctx_copy, .node = &node}}, DeclKind_Unresolved};
}

void defineLocal(Context &ctx, NkAtom name, NkIrLocalVar var) {
    makeDecl(ctx, name) = {{.val{{.local{var}}, ValueKind_Local}}, DeclKind_Complete};
}

void defineParam(Context &ctx, NkAtom name, usize idx) {
    makeDecl(ctx, name) = {{.val{{.arg{idx}}, ValueKind_Arg}}, DeclKind_Complete};
}

void defineExternProc(Context &ctx, NkAtom name, NkIrExternProc id) {
    makeDecl(ctx, name) = {{.val{{.extern_proc{id}}, ValueKind_ExternProc}}, DeclKind_Complete};
}

void defineExternData(Context &ctx, NkAtom name, NkIrExternData id) {
    makeDecl(ctx, name) = {{.val{{.extern_data{id}}, ValueKind_ExternData}}, DeclKind_Complete};
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
           (val.kind == IntermKind_Val && (val.as.val.kind == ValueKind_Proc || val.as.val.kind == ValueKind_Rodata));
}

nklval_t getValueFromInterm(NklCompiler c, Interm const &val) {
    nk_assert(isValueKnown(val) && "trying to get an unknown value");

    switch (val.kind) {
        case IntermKind_Void:
            return {nullptr, val.type};
        case IntermKind_Val:
            switch (val.as.val.kind) {
                case ValueKind_Rodata:
                    return {nkir_getDataPtr(c->ir, val.as.val.as.rodata.id), val.type};
                case ValueKind_Proc:
                    nk_assert(!"getValueFromInfo is not implemented for ValueKind_Proc");
                    return {};

                case ValueKind_Void:
                case ValueKind_Data:
                case ValueKind_ExternData:
                case ValueKind_ExternProc:
                case ValueKind_Local:
                case ValueKind_Arg:
                    nk_assert(!"unreachable");
                    return {};
            }
            return {};

        case IntermKind_Instr:
        case IntermKind_Ref:
            nk_assert(!"unreachable");
            return {};
    }

    nk_assert(!"unreachable");
    return {};
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

nkltype_t getValueType(NklCompiler c, Value const &val) {
    switch (val.kind) {
        case ValueKind_Void:
            return nkl_get_void(c->nkl);
        case ValueKind_Rodata:
            return nkirt2nklt(nkir_getDataType(c->ir, val.as.rodata.id));
        case ValueKind_Proc:
            return nkirt2nklt(nkir_getProcType(c->ir, val.as.proc.id));
        case ValueKind_Data:
            return nkirt2nklt(nkir_getDataType(c->ir, val.as.data.id));
        case ValueKind_ExternData:
            return nkirt2nklt(nkir_getExternDataType(c->ir, val.as.extern_data.id));
        case ValueKind_ExternProc:
            return nkirt2nklt(nkir_getExternProcType(c->ir, val.as.extern_proc.id));
        case ValueKind_Local:
            return nkirt2nklt(nkir_getLocalType(c->ir, val.as.local.id));
        case ValueKind_Arg:
            return nkirt2nklt(nkir_getArgType(c->ir, val.as.arg.idx));
    }

    nk_assert(!"unreachable");
    return {};
}
