#include "nkl/lang/compiler.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <unordered_map>
#include <vector>

#include "ast_impl.h"
#include "nk/common/allocator.h"
#include "nk/common/id.h"
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

enum EDeclKind {
    Decl_Undefined,

    Decl_ComptimeConst,
    Decl_Local,
    Decl_Global,
    Decl_Funct,
    Decl_ExtSym,
    Decl_Arg,
};

enum EComptimeConstKind {
    ComptimeConst_Value,
    ComptimeConst_Funct,
};

struct ComptimeConst {
    union {
        nkval_t value;
        NkIrFunct funct;
    };
    EComptimeConstKind kind;
};

struct Decl { // TODO Compact the Decl struct
    union {
        ComptimeConst comptime_const;
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
    EDeclKind kind;
};

enum EValueKind {
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
        Decl *decl;
    } as;
    nktype_t type;
    EValueKind kind;
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

    size_t funct_counter{};
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
        std::abort(); // TODO Report errors properly
    } else {
        std::tie(it, std::ignore) = locals.emplace(name, Decl{});
    }

    return it->second;
}

void defineComptimeConst(NklCompiler c, nkid name, ComptimeConst cnst) {
    NK_LOG_DBG("defining comptime const `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.comptime_const{cnst}}, Decl_ComptimeConst};
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

Decl &resolve(NklCompiler c, nkid name) {
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
    static Decl s_undefined_decl{{}, Decl_Undefined};
    return s_undefined_decl;
}

template <class T, class... TArgs>
ValueInfo makeValue(NklCompiler c, nktype_t type, TArgs &&... args) {
    return {{.val = new (nk_allocate(c->arena, sizeof(T))) T{args...}}, type, v_val};
}

ValueInfo makeValue(NklCompiler, nkval_t val) {
    return {{.val = nkval_data(val)}, val.type, v_val};
}

ValueInfo makeInstr(NkIrInstr const &instr, nktype_t type) {
    return {{.instr{instr}}, type, v_instr};
}

nkval_t comptimeConstGetValue(NklCompiler c, ComptimeConst &cnst) {
    switch (cnst.kind) {
    case ComptimeConst_Value:
        NK_LOG_DBG("returning comptime const as value");
        return cnst.value;
    case ComptimeConst_Funct: {
        NK_LOG_DBG("getting comptime const from funct");
        auto fn_t = nkir_functGetType(cnst.funct);
        auto type = fn_t->as.fn.ret_t;
        nkval_t val{nk_allocate(c->arena, type->size), type};
        nkir_invoke({&cnst.funct, fn_t}, val, {});
        cnst.value = val;
        cnst.kind = ComptimeConst_Value;
        return val;
    }
    }
}

bool isKnown(ValueInfo const &val) {
    return val.kind == v_val || (val.kind == v_decl && val.as.decl->kind == Decl_ComptimeConst);
}

nkval_t asValue(NklCompiler c, ValueInfo const &val) {
    assert(isKnown(val) && "getting unknown value ");
    if (val.kind == v_val) {
        return {val.as.val, val.type};
    } else {
        return comptimeConstGetValue(c, val.as.decl->as.comptime_const);
    }
}

nktype_t comptimeConstType(ComptimeConst cnst) {
    switch (cnst.kind) {
    case ComptimeConst_Value:
        return nkval_typeof(cnst.value);
    case ComptimeConst_Funct:
        return nkir_functGetType(cnst.funct)->as.fn.ret_t;
    }
}

NkIrRef makeRef(NklCompiler c, ValueInfo const &val) {
    switch (val.kind) {
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
        switch (decl.kind) {
        case Decl_ComptimeConst:
            return nkir_makeConstRef(c->ir, comptimeConstGetValue(c, decl.as.comptime_const));
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

ValueInfo makeRefAndStore(NklCompiler c, NkIrRef const &dst, ValueInfo val) {
    assert(val.type->typeclass_id != NkType_Void && "storing void");

    if (val.kind == v_instr) {
        auto &instr_dst = val.as.instr.arg[0].ref;
        if (instr_dst.ref_type == NkIrRef_None) {
            instr_dst = dst;
            return {{.ref = makeRef(c, val)}, dst.type, v_ref};
        }
    }

    gen(c, nkir_make_mov(dst, makeRef(c, val)));

    return {{.ref = dst}, dst.type, v_ref};
}

ComptimeConst comptimeCompileNode(NklCompiler c, NklAstNode node);
nkval_t comptimeCompileNodeGetValue(NklCompiler c, NklAstNode node);
ValueInfo compileNode(NklCompiler c, NklAstNode node);
void compileStmt(NklCompiler c, NklAstNode node);
void compileNodeArray(NklCompiler c, NklAstNodeArray nodes);

#define COMPILE(NAME) ValueInfo _compile_##NAME(NklCompiler c, NklAstNode node)

COMPILE(none) {
    (void)c;
    (void)node;
    return {};
}

COMPILE(u8) {
    (void)node;
    // TODO Modeling type_t as *void
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Uint8));
}

COMPILE(u16) {
    (void)node;
    // TODO Modeling type_t as *void
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Uint16));
}

COMPILE(u32) {
    (void)node;
    // TODO Modeling type_t as *void
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Uint32));
}

COMPILE(u64) {
    (void)node;
    // TODO Modeling type_t as *void
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Uint64));
}

COMPILE(void) {
    (void)node;
    // TODO Modeling type_t as *void
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_void(c->arena));
}

COMPILE(return ) {
    auto arg = compileNode(c, node->args[0].data);
    makeRefAndStore(c, nkir_makeRetRef(c->ir), arg);
    gen(c, nkir_make_ret());
    return makeVoid(c);
}

COMPILE(ptr_type) {
    auto target_type = comptimeCompileNodeGetValue(c, node->args[0].data);
    // TODO Modeling type_t as *void
    return makeValue<nktype_t>(
        c,
        nkt_get_ptr(c->arena, nkt_get_void(c->arena)),
        nkt_get_ptr(c->arena, nkval_as(nktype_t, target_type)));
}

COMPILE(scope) {
    assert(!"scope compilation is not implemented");
}

COMPILE(add) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    // TODO if (lhs.type->id != rhs.type->id) {
    //     NK_LOG_ERR("cannot add values of different types");
    //     std::abort(); // TODO Report errors properly
    // }
    return makeInstr(nkir_make_add({}, makeRef(c, lhs), makeRef(c, rhs)), lhs.type);
}

COMPILE(sub) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    // TODO if (lhs.type->id != rhs.type->id) {
    //     NK_LOG_ERR("cannot sub values of different types");
    //     std::abort(); // TODO Report errors properly
    // }
    return makeInstr(nkir_make_sub({}, makeRef(c, lhs), makeRef(c, rhs)), lhs.type);
}

COMPILE(mul) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    // TODO if (lhs.type->id != rhs.type->id) {
    //     NK_LOG_ERR("cannot mul values of different types");
    //     std::abort(); // TODO Report errors properly
    // }
    return makeInstr(nkir_make_mul({}, makeRef(c, lhs), makeRef(c, rhs)), lhs.type);
}

COMPILE(div) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    // TODO if (lhs.type->id != rhs.type->id) {
    //     NK_LOG_ERR("cannot div values of different types");
    //     std::abort(); // TODO Report errors properly
    // }
    return makeInstr(nkir_make_div({}, makeRef(c, lhs), makeRef(c, rhs)), lhs.type);
}

COMPILE(mod) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    // TODO if (lhs.type->id != rhs.type->id) {
    //     NK_LOG_ERR("cannot mod values of different types");
    //     std::abort(); // TODO Report errors properly
    // }
    return makeInstr(nkir_make_mod({}, makeRef(c, lhs), makeRef(c, rhs)), lhs.type);
}

COMPILE(eq) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    // TODO if (lhs.type->id != rhs.type->id) {
    //     NK_LOG_ERR("cannot eq values of different types");
    //     std::abort(); // TODO Report errors properly
    // }
    return makeInstr(nkir_make_eq({}, makeRef(c, lhs), makeRef(c, rhs)), lhs.type);
}

COMPILE(ne) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    // TODO if (lhs.type->id != rhs.type->id) {
    //     NK_LOG_ERR("cannot ne values of different types");
    //     std::abort(); // TODO Report errors properly
    // }
    return makeInstr(nkir_make_ne({}, makeRef(c, lhs), makeRef(c, rhs)), lhs.type);
}

COMPILE(while) {
    auto l_loop = nkir_makeBlock(c->ir);
    auto l_endloop = nkir_makeBlock(c->ir);
    nkir_startBlock(c->ir, l_loop, cs2s("loop"));
    gen(c, nkir_make_enter());
    auto cond = compileNode(c, node->args[0].data);
    gen(c, nkir_make_jmpz(makeRef(c, cond), l_endloop));
    compileStmt(c, node->args[1].data);
    gen(c, nkir_make_leave());
    gen(c, nkir_make_jmp(l_loop));
    nkir_startBlock(c->ir, l_endloop, cs2s("endloop"));
    return makeVoid(c);
}

COMPILE(if) {
    auto l_endif = nkir_makeBlock(c->ir);
    NkIrBlockId l_else;
    if (node->args[2].data) {
        l_else = nkir_makeBlock(c->ir);
    } else {
        l_else = l_endif;
    }
    auto cond = compileNode(c, node->args[0].data);
    gen(c, nkir_make_jmpz(makeRef(c, cond), l_else));
    compileStmt(c, node->args[1].data);
    if (node->args[2].data) {
        gen(c, nkir_make_jmp(l_endif));
        nkir_startBlock(c->ir, l_else, cs2s("else"));
        compileStmt(c, node->args[2].data);
    }
    nkir_startBlock(c->ir, l_endif, cs2s("endif"));
    return makeVoid(c);
}

COMPILE(block) {
    compileNodeArray(c, node->args[0]);
    return makeVoid(c);
}

COMPILE(import) {
    return makeVoid(c); // TODO import not implemented
}

COMPILE(id) {
    nkstr name_str = node->token->text;
    nkid name = s2nkid(name_str);
    auto &decl = resolve(c, name);
    switch (decl.kind) {
    case Decl_Undefined:
        NK_LOG_ERR("`%.*s` is not defined", name_str.size, name_str.data);
        std::abort(); // TODO Report errors properly
    case Decl_ComptimeConst:
        return {{.decl = &decl}, comptimeConstType(decl.as.comptime_const), v_decl};
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

COMPILE(float) {
    double value = 0.0;
    // TODO Replace sscanf in Compiler
    int res = std::sscanf(node->token->text.data, "%lf", &value);
    (void)res;
    assert(res > 0 && res != EOF && "integer constant parsing failed");
    return makeValue<double>(c, nkt_get_numeric(c->arena, Float64), value);
}

COMPILE(int) {
    int64_t value = 0;
    // TODO Replace sscanf in Compiler
    int res = std::sscanf(node->token->text.data, "%ld", &value);
    (void)res;
    assert(res > 0 && res != EOF && "integer constant parsing failed");
    return makeValue<int64_t>(c, nkt_get_numeric(c->arena, Int64), value);
}

COMPILE(string) {
    auto size = node->token->text.size;

    auto u8_t = nkt_get_numeric(c->arena, Uint8);
    auto ar_t = nkt_get_array(c->arena, u8_t, size + 1);
    auto str_t = nkt_get_ptr(c->arena, ar_t);

    auto str = (char *)nk_allocate(c->arena, size + 1);
    std::memcpy(str, node->token->text.data, size);
    str[size] = '\0';

    return makeValue<char *>(c, str_t, str);
}

COMPILE(member) {
    return makeVoid(c); // TODO member not implemented
}

COMPILE(fn) {
    // TODO Refactor fn compilation

    std::vector<nkid> params_names;
    std::vector<nktype_t> params_types;

    auto params = node->args[0];

    for (size_t i = 0; i < params.size; i++) {
        auto const &param = params.data[i];
        params_names.emplace_back(s2nkid(param.args[0].data->token->text));
        params_types.emplace_back(
            nkval_as(nktype_t, comptimeCompileNodeGetValue(c, param.args[1].data)));
    }

    nktype_t ret_t = nkval_as(nktype_t, comptimeCompileNodeGetValue(c, node->args[1].data));
    nktype_t args_t = nkt_get_tuple(c->arena, params_types.data(), params_types.size(), 1);

    NktFnInfo fn_info{ret_t, args_t, NkCallConv_Nk, false};
    auto fn_t = nkt_get_fn(c->arena, &fn_info);

    auto prev_fn = c->cur_fn;
    defer {
        c->cur_fn = prev_fn;
        nkir_activateFunct(c->ir, c->cur_fn);
    };

    auto fn = nkir_makeFunct(c->ir);
    c->cur_fn = fn;

    char fn_name_buf[100];
    std::snprintf(
        fn_name_buf,
        sizeof(fn_name_buf),
        "funct_%lu",
        c->funct_counter++); // TODO snprintf in compiler
    nkir_startFunct(fn, cs2s(fn_name_buf), fn_t);
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

COMPILE(fn_type) {
    std::vector<nkid> params_names;
    std::vector<nktype_t> params_types;

    auto params = node->args[0];

    for (size_t i = 0; i < params.size; i++) {
        auto const &param = params.data[i];
        params_names.emplace_back(s2nkid(param.args[0].data->token->text));
        params_types.emplace_back(
            nkval_as(nktype_t, comptimeCompileNodeGetValue(c, param.args[1].data)));
    }

    nktype_t ret_t = nkval_as(nktype_t, comptimeCompileNodeGetValue(c, node->args[1].data));
    nktype_t args_t = nkt_get_tuple(c->arena, params_types.data(), params_types.size(), 1);

    NktFnInfo fn_info{ret_t, args_t, NkCallConv_Cdecl, false}; // TODO CallConv Hack for #foreign
    auto fn_t = nkt_get_fn(c->arena, &fn_info);

    // TODO Modeling type_t as *void
    return makeValue<nktype_t>(c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), fn_t);
}

COMPILE(tag) {
    // TODO Only handling #foreign
    assert(
        0 == std::strncmp(
                 node->args[0].data->token->text.data,
                 "#foreign",
                 node->args[0].data->token->text.size));

    auto name = compileNode(c, node->args[1].data);

    assert(node->args[2].data->id == n_comptime_const_def);

    nkstr soname{
        nkval_as(char *, asValue(c, name)), name.type->as.ptr.target_type->as.arr.elem_count};
    auto so = nkir_makeShObj(c->ir, soname); // TODO Creating so every time

    nkstr sym_name{node->args[2].data->args[0].data->token->text};

    auto fn_t_val = compileNode(c, node->args[2].data->args[1].data);
    auto fn_t = nkval_as(nktype_t, asValue(c, fn_t_val));

    defineExtSym(c, s2nkid(sym_name), nkir_makeExtSym(c->ir, so, sym_name, fn_t), fn_t);

    return makeVoid(c);
}

COMPILE(call) {
    auto lhs = compileNode(c, node->args[0].data);

    auto fn_t = lhs.type;

    NkIrRef args{};
    if (fn_t->as.fn.args_t->size) {
        args = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, fn_t->as.fn.args_t));
    }

    for (size_t i = 0; i < node->args[1].size; i++) {
        auto arg = compileNode(c, &node->args[1].data[i]);
        auto arg_ref = args;
        arg_ref.offset += fn_t->as.fn.args_t->as.tuple.elems.data[i].offset;
        arg_ref.type = fn_t->as.fn.args_t->as.tuple.elems.data[i].type;
        makeRefAndStore(c, arg_ref, arg);
    }

    return makeInstr(nkir_make_call({}, makeRef(c, lhs), args), fn_t->as.fn.ret_t);
}

COMPILE(assign) {
    auto name_str = node->args[0].data->token->text;
    nkid name = s2nkid(name_str);
    auto rhs = compileNode(c, node->args[1].data);
    auto res = resolve(c, name);
    NkIrRef ref;
    nktype_t type;
    switch (res.kind) {
    case Decl_Local:
        if (res.as.local.type->id != rhs.type->id) {
            // return error("cannot assign values of different types"), ValueInfo{};
        }
        ref = nkir_makeFrameRef(c->ir, res.as.local.id);
        type = res.as.local.type;
        break;
    case Decl_Global:
        if (res.as.local.type->id != rhs.type->id) {
            // return error("cannot assign values of different types"), ValueInfo{};
        }
        ref = nkir_makeGlobalRef(c->ir, res.as.global.id);
        type = res.as.global.type;
        break;
    case Decl_Undefined:
        // return error("`%.*s` is not defined", name_str.size, name_str.data), ValueInfo{};
    case Decl_Funct:
    case Decl_ExtSym:
    case Decl_Arg:
        // return error("cannot assign to `%.*s`", name_str.size, name_str.data), ValueInfo{};
    default:
        NK_LOG_ERR("unknown decl type");
        assert(!"unreachable");
        return {};
    }
    return makeRefAndStore(c, ref, rhs);
}

COMPILE(define) {
    auto const &names = node->args[0];
    if (names.size > 1) {
        NK_LOG_ERR("multiple assignment is not implemented");
        std::abort(); // TODO Report errors properly
    }
    nkid name = s2nkid(names.data[0].token->text);
    auto rhs = compileNode(c, node->args[1].data);
    NkIrRef ref;
    if (c->is_top_level) {
        auto var = nkir_makeGlobalVar(c->ir, rhs.type);
        defineGlobal(c, name, var, rhs.type);
        ref = nkir_makeGlobalRef(c->ir, var);
    } else {
        auto var = nkir_makeLocalVar(c->ir, rhs.type);
        defineLocal(c, name, var, rhs.type);
        ref = nkir_makeFrameRef(c->ir, var);
    }
    makeRefAndStore(c, ref, rhs);
    return makeVoid(c);
}

COMPILE(comptime_const_def) {
    auto names = node->args[0];
    if (names.size > 1) {
        NK_LOG_ERR("multiple assignment is not implemented");
        std::abort(); // TODO Report errors properly
    }
    nkid name = s2nkid(names.data[0].token->text);
    auto decl = comptimeCompileNode(c, node->args[1].data);
    defineComptimeConst(c, name, decl);
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

ComptimeConst comptimeCompileNode(NklCompiler c, NklAstNode node) {
    auto fn = nkir_makeFunct(c->ir);
    NktFnInfo fn_info{nullptr, nkt_get_tuple(c->arena, nullptr, 0, 1), NkCallConv_Nk, false};

    auto prev_fn = c->cur_fn;
    defer {
        c->cur_fn = prev_fn;
        nkir_activateFunct(c->ir, c->cur_fn);
    };

    c->cur_fn = fn;

    nkir_startIncompleteFunct(fn, cs2s("#comptime_const_getter"), &fn_info);
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), cs2s("start"));

    pushScope(c);
    defer {
        popScope(c);
    };

    auto cnst_val = compileNode(c, node);
    nkir_incompleteFunctGetInfo(fn)->ret_t = cnst_val.type;
    nkir_finalizeIncompleteFunct(fn, c->arena);

    ComptimeConst cnst{};

    if (isKnown(cnst_val)) {
        nkir_discardFunct(fn);

        cnst.value = asValue(c, cnst_val);
        cnst.kind = ComptimeConst_Value;
    } else {
        makeRefAndStore(c, nkir_makeRetRef(c->ir), cnst_val);
        gen(c, nkir_make_ret());

        cnst.funct = fn;
        cnst.kind = ComptimeConst_Funct;
    }

    return cnst;
}

nkval_t comptimeCompileNodeGetValue(NklCompiler c, NklAstNode node) {
    auto cnst = comptimeCompileNode(c, node);
    return comptimeConstGetValue(c, cnst);
}

void compileStmt(NklCompiler c, NklAstNode node) {
    auto val = compileNode(c, node);
    auto ref = makeRef(c, val);
    if (val.kind != v_none) {
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

NklCompiler nkl_compiler_create(NklCompilerConfig config) {
    // TODO config unused
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
    NktFnInfo top_level_fn_info{
        nkt_get_void(c->arena), nkt_get_tuple(c->arena, nullptr, 0, 1), NkCallConv_Nk, false};
    auto top_level_fn_t = nkt_get_fn(c->arena, &top_level_fn_info);

    nkir_startFunct(top_level_fn, cs2s("#top_level"), top_level_fn_t);
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

void nkl_compiler_runFile(NklCompiler c, nkstr filename) {
}
