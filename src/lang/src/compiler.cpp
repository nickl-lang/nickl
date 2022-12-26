#include "nkl/lang/compiler.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <unordered_map>
#include <vector>

#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/string.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/value.h"

namespace {

NK_LOG_USE_SCOPE(compiler);

enum EDeclType {
    Decl_Undefined,

    Decl_ComptimeConst,
    Decl_Local,
    Decl_Global,
    Decl_Funct,
    Decl_ExtSym,
    Decl_Arg,
};

struct Decl {
    union {
        struct {
            nkval_t value;
        } comptime_const;
        struct {
            NkIrLocalVarId id;
            nktype_t type;
        } local;
        struct {
            NkIrGlobalVarId id;
            nktype_t type;
        } global;
        struct {
            NkIrFunct id;
            nktype_t fn_t;
        } funct;
        struct {
            NkIrExtSymId id;
            nktype_t type;
        } ext_sym;
        struct {
            size_t index;
            nktype_t type;
        } arg;
    } as;
    EDeclType decl_type;
};

enum EValueType {
    v_none,

    v_val,
    v_ref,
    v_instr,
    v_decl,
};

struct ValueInfo {
    union Var {
        void *val;
        NkIrRef ref;
        NkIrInstr instr;
        Decl const *decl;
    } as;
    nktype_t type;
    EValueType value_type;
};

struct ScopeCtx {
    std::unordered_map<nkid, Decl> locals;
};

} // namespace

struct NklCompiler_T {
    NkIrProg ir;
    NkAllocator *arena;

    std::vector<ScopeCtx> scopes{};
};

namespace {

ScopeCtx &curScope(NklCompiler c) {
    assert(!c->scopes.empty() && "no current scope");
    return c->scopes.back();
}

void pushScope(NklCompiler c) {
    NK_LOG_DBG("entering scope=%lu", c->scopes.size());
    c->scopes.emplace_back();
}

void popScope(NklCompiler c) {
    c->scopes.pop_back();
    NK_LOG_DBG("exiting scope=%lu", c->scopes.size());
}

void gen(NklCompiler c, NkIrInstr const &instr) {
    NK_LOG_DBG("gen: %s", s_nk_ir_names[instr.code]);
    nkir_gen(c->ir, instr);
}

ValueInfo makeVoid(NklCompiler c) {
    return {{}, nkt_get_void(c->arena), v_none};
}

Decl &makeDecl(NklCompiler c, nkid name) {
    auto &locals = curScope(c).locals;

    NK_LOG_DBG(
        "making declaration name=`%.*s` scope=%lu",
        nkid2s(name).size,
        nkid2s(name).data,
        c->scopes.size() - 1);

    auto it = locals.find(name);
    if (it != locals.end()) {
        nkstr name_str = nkid2s(name);
        NK_LOG_ERR("`%.*s` is already defined", name_str.size, name_str.data);
        throw "whoops"; // TODO Report errors properly
    } else {
        std::tie(it, std::ignore) = locals.emplace(name, Decl{});
    }

    return it->second;
}

void defineLocal(NklCompiler c, nkid name, NkIrLocalVarId id, nktype_t type) {
    NK_LOG_DBG("defining local `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.local = {id, type}}, Decl_Local};
}

void defineGlobal(NklCompiler c, nkid name, NkIrGlobalVarId id, nktype_t type) {
    NK_LOG_DBG("defining global `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.global = {id, type}}, Decl_Global};
}

void defineFunct(NklCompiler c, nkid name, NkIrFunct funct, nktype_t fn_t) {
    NK_LOG_DBG("defining funct `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.funct = {.id = funct, .fn_t = fn_t}}, Decl_Funct};
}

void defineExtSym(NklCompiler c, nkid name, NkIrExtSymId id, nktype_t type) {
    NK_LOG_DBG("defining ext sym `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.ext_sym = {.id = id, .type = type}}, Decl_ExtSym};
}

void defineArg(NklCompiler c, nkid name, size_t index, nktype_t type) {
    NK_LOG_DBG("defining arg `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.arg = {index, type}}, Decl_Arg};
}

Decl const &resolve(NklCompiler c, nkid name) {
    NK_LOG_DBG(
        "resolving name=`%.*s` scope=%lu",
        nkid2s(name).size,
        nkid2s(name).data,
        c->scopes.size() - 1);

    for (size_t i = c->scopes.size(); i > 0; i--) {
        auto &scope = c->scopes[i - 1];
        auto it = scope.locals.find(name);
        if (it != scope.locals.end()) {
            return it->second;
        }
    }
    static Decl const s_undefined_decl{{}, Decl_Undefined};
    return s_undefined_decl;
}

template <class T, class... TArgs>
ValueInfo makeValue(NklCompiler c, nktype_t type, TArgs &&... args) {
    return {{.val = new (nk_allocate(c->arena, sizeof(T))) T{args...}}, type, v_val};
}

ValueInfo makeInstr(NkIrInstr const &instr, nktype_t type) {
    return {{.instr{instr}}, type, v_instr};
}

nkval_t asValue(ValueInfo &val) {
    return {val.as.val, val.type};
}

NkIrRef makeRef(NklCompiler c, ValueInfo const &val) {
    switch (val.value_type) {
    case v_val: // TODO Isn't it the same as Decl_ComptimeConst?
        return nkir_makeConstRef(c->ir, {val.as.val, val.type});

    case v_ref:
        return val.as.ref;

    case v_instr: {
        auto instr = val.as.instr;
        auto &dst = instr.arg[0].ref;
        if (dst.ref_type == NkIrRef_None && val.type->typeclass_id != NkType_Void) {
            dst = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, val.type));
        }
        gen(c, instr);
        return dst;
    }

    case v_decl: {
        auto &decl = *val.as.decl;
        switch (decl.decl_type) {
        case Decl_ComptimeConst:
            return nkir_makeConstRef(c->ir, decl.as.comptime_const.value);
        case Decl_Local:
            return nkir_makeFrameRef(c->ir, decl.as.local.id);
        case Decl_Global:
            return nkir_makeGlobalRef(c->ir, decl.as.global.id);
        case Decl_Funct:
            return nkir_makeFunctRef(decl.as.funct.id);
        case Decl_ExtSym:
            return nkir_makeExtSymRef(c->ir, decl.as.ext_sym.id);
        case Decl_Arg:
            return nkir_makeArgRef(c->ir, decl.as.arg.index);
        case Decl_Undefined:
            assert(!"referencing an undefined declaration");
        default:
            assert(!"unreachable");
            return {};
        }
    }

    default:
        return {};
    };
}

} // namespace

NklCompiler nkl_compiler_create() {
    return new (nk_allocate(nk_default_allocator, sizeof(NklCompiler_T))) NklCompiler_T{
        .ir = nkir_createProgram(),
        .arena = nk_create_arena(),
    };
}

void nkl_compiler_free(NklCompiler c) {
    nk_free_arena(c->arena);
    nkir_deinitProgram(c->ir);
    c->~NklCompiler_T();
    nk_free(nk_default_allocator, c);
}

void nkl_compiler_run(NklCompiler c, NklAstNode root) {
    NK_LOG_TRC(__func__);

    auto top_level_fn = nkir_makeFunct(c->ir);
    auto top_level_fn_t = nkt_get_fn(
        c->arena,
        nkt_get_void(c->arena),
        nkt_get_tuple(c->arena, nullptr, 0, 1),
        NkCallConv_Nk,
        false);

    nkir_startFunct(c->ir, top_level_fn, cs2s("#top_level"), top_level_fn_t);
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), cs2s("start"));
    gen(c, nkir_make_ret());

    {
        // TODO Inspecting ir in nkl_compiler_run outside of the log macro
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };

        nkir_inspect(c->ir, sb);
        auto str = nksb_concat(sb);

        NK_LOG_INF("ir:\n%.*s", str.size, str.data);
    }

    nkir_invoke({&top_level_fn, top_level_fn_t}, {}, {});
}
