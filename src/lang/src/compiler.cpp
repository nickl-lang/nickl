#include "nkl/lang/compiler.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <unordered_map>
#include <vector>

#include "ast_impl.h"
#include "nk/common/allocator.h"
#include "nk/common/logger.h"
#include "nk/common/string.h"
#include "nk/common/string_builder.h"
#include "nk/common/utils.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/value.h"
#include "nkl/lang/ast.h"

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

struct Decl { // TODO Compact the Decl struct
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

    NkIrFunct cur_fn;
    bool is_top_level{};
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

void defineComptimeConst(NklCompiler c, nkid name, nkval_t val) {
    NK_LOG_DBG("defining comptime const `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.comptime_const{val}}, Decl_ComptimeConst};
}

void defineLocal(NklCompiler c, nkid name, NkIrLocalVarId id, nktype_t type) {
    NK_LOG_DBG("defining local `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.local{id, type}}, Decl_Local};
}

void defineGlobal(NklCompiler c, nkid name, NkIrGlobalVarId id, nktype_t type) {
    NK_LOG_DBG("defining global `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.global{id, type}}, Decl_Global};
}

void defineFunct(NklCompiler c, nkid name, NkIrFunct funct, nktype_t fn_t) {
    NK_LOG_DBG("defining funct `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.funct{funct, fn_t}}, Decl_Funct};
}

void defineExtSym(NklCompiler c, nkid name, NkIrExtSymId id, nktype_t type) {
    NK_LOG_DBG("defining ext sym `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.ext_sym{.id = id, .type = type}}, Decl_ExtSym};
}

void defineArg(NklCompiler c, nkid name, size_t index, nktype_t type) {
    NK_LOG_DBG("defining arg `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.arg{index, type}}, Decl_Arg};
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

nkval_t asValue(ValueInfo const &val) {
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

ValueInfo compileNode(NklCompiler c, NklAstNode node);
void compileStmt(NklCompiler c, NklAstNode node);
void compileNodeArray(NklCompiler c, NklAstNodeArray nodes);

#define COMPILE(NAME) ValueInfo _compile_##NAME(NklCompiler c, NklAstNode node)

COMPILE(none) {
    (void)c;
    (void)node;
    return {};
}

COMPILE(u32) {
    // TODO Modeling type_t as *void
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Uint32));
}

COMPILE(return ) {
    auto arg = compileNode(c, node->args[0].data);
    gen(c, nkir_make_mov(nkir_makeRetRef(c->ir), makeRef(c, arg)));
    gen(c, nkir_make_ret());
    return makeVoid(c);
}

COMPILE(add) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    // TODO if (lhs.type->id != rhs.type->id) {
    //     NK_LOG_ERR("cannot add values of different types");
    //     throw "whoops"; // TODO Report errors properly
    // }
    return makeInstr(nkir_make_add({}, makeRef(c, lhs), makeRef(c, rhs)), lhs.type);
}

COMPILE(block) {
    compileNodeArray(c, node->args[0]);
    return makeVoid(c);
}

COMPILE(id) {
    nkstr name_str = node->token->text;
    nkid name = s2nkid(name_str);
    auto const &decl = resolve(c, name);
    switch (decl.decl_type) {
    case Decl_Undefined:
        NK_LOG_ERR("`%.*s` is not defined", name_str.size, name_str.data);
        throw "whoops"; // TODO Report errors properly
    case Decl_ComptimeConst:
        return {{.decl = &decl}, decl.as.comptime_const.value.type, v_decl};
    case Decl_Local:
        return {{.decl = &decl}, decl.as.local.type, v_decl};
    case Decl_Global:
        return {{.decl = &decl}, decl.as.global.type, v_decl};
    case Decl_Funct:
        return {{.decl = &decl}, decl.as.funct.fn_t, v_decl};
    case Decl_ExtSym:
        return {{.decl = &decl}, decl.as.ext_sym.type, v_decl};
    case Decl_Arg:
        return {{.decl = &decl}, decl.as.arg.type, v_decl};
    default:
        NK_LOG_ERR("unknown decl type");
        assert(!"unreachable");
        return {};
    }
}

COMPILE(int) {
    int64_t value = 0;
    // TODO Replace sscanf in Compiler
    int res = std::sscanf(node->token->text.data, "%ld", &value);
    (void)res;
    assert(res > 0 && res != EOF && "integer constant parsing failed");
    return makeValue<int64_t>(c, nkt_get_numeric(c->arena, Int64), value);
}

COMPILE(fn) {
    // TODO Refactor fn compilation

    std::vector<nkid> params_names;
    std::vector<nktype_t> params_types;

    auto params = node->args[0];

    for (size_t i = 0; i < params.size; i++) {
        auto const &param = params.data[i];
        params_names.emplace_back(s2nkid(param.args[0].data->token->text));
        params_types.emplace_back(nkval_as(nktype_t, asValue(compileNode(c, param.args[1].data))));
    }

    auto ret = compileNode(c, node->args[1].data);

    nktype_t ret_t = nkval_as(nktype_t, asValue(ret));
    nktype_t args_t = nkt_get_tuple(c->arena, params_types.data(), params_types.size(), 1);

    auto fn_t = nkt_get_fn(c->arena, ret_t, args_t, NkCallConv_Nk, false);

    auto prev_fn = c->cur_fn;
    defer {
        c->cur_fn = prev_fn;
        nkir_activateFunct(c->ir, c->cur_fn);
    };

    auto fn = nkir_makeFunct(c->ir);
    c->cur_fn = fn;

    nkir_startFunct(c->ir, fn, cs2s(""), fn_t); // TODO Empty funct name
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), cs2s("start"));

    pushScope(c);
    defer {
        popScope(c);
    };

    for (size_t i = 0; i < params.size; i++) {
        defineArg(c, params_names[i], i, params_types[i]);
    }

    compileStmt(c, node->args[2].data);

    return makeValue<void *>(c, fn_t, fn);
}

COMPILE(call) {
    auto lhs = compileNode(c, node->args[0].data);

    auto fn_t = lhs.type;

    auto args = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, fn_t->as.fn.args_t));

    for (size_t i = 0; i < node->args[1].size; i++) {
        auto arg = compileNode(c, &node->args[1].data[i]);
        auto arg_ref = args;
        arg_ref.offset += fn_t->as.fn.args_t->as.tuple.elems.data[i].offset;
        arg_ref.type = fn_t->as.fn.args_t->as.tuple.elems.data[i].type;
        gen(c, nkir_make_mov(arg_ref, makeRef(c, arg)));
    }

    return makeInstr(nkir_make_call({}, makeRef(c, lhs), args), fn_t->as.fn.ret_t);
}

COMPILE(const_decl) {
    auto names = node->args[0];
    if (names.size > 1) {
        NK_LOG_ERR("multiple assignment is not implemented");
        throw "whoops"; // TODO Report errors properly
    }
    nkid name = s2nkid(names.data[0].token->text);
    auto rhs = compileNode(c, node->args[1].data);
    defineComptimeConst(c, name, asValue(rhs)); // TODO Unconditionally treating rhs as value
    return makeVoid(c);
}

using CompileFunc = ValueInfo (*)(NklCompiler c, NklAstNode node);

CompileFunc s_funcs[] = {
#define X(NAME) CAT(_compile_, NAME),
#include "nodes.inl"
};

ValueInfo compileNode(NklCompiler c, NklAstNode node) {
    assert(node->id < AR_SIZE(s_funcs) && "invalid node");
    NK_LOG_DBG("node: %s", s_nkl_ast_node_names[node->id]);
    return s_funcs[node->id](c, node);
}

void compileStmt(NklCompiler c, NklAstNode node) {
    auto val = compileNode(c, node);
    auto ref = makeRef(c, val);
    if (val.value_type != v_none) {
        (void)ref;
        // TODO Boilerplate for debug printing
#ifdef ENABLE_LOGGING
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };
#endif // ENABLE_LOGGING
        NK_LOG_DBG("value ignored: %s", [&]() {
            nkir_inspectRef(c->ir, ref, sb);
            return nksb_concat(sb).data;
        }());
    }
}

void compileNodeArray(NklCompiler c, NklAstNodeArray nodes) {
    for (size_t i = 0; i < nodes.size; i++) {
        compileStmt(c, &nodes.data[i]);
    }
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

    c->is_top_level = true;

    auto top_level_fn = nkir_makeFunct(c->ir);
    auto top_level_fn_t = nkt_get_fn(
        c->arena,
        nkt_get_void(c->arena),
        nkt_get_tuple(c->arena, nullptr, 0, 1),
        NkCallConv_Nk,
        false);

    nkir_startFunct(c->ir, top_level_fn, cs2s("#top_level"), top_level_fn_t);
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), cs2s("start"));

    pushScope(c);
    defer {
        popScope(c);
    };

    if (root) {
        c->cur_fn = top_level_fn;
        compileStmt(c, root);
    }

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
