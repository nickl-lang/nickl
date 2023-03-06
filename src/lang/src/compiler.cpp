#include "nkl/lang/compiler.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <new>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast_impl.h"
#include "lexer.hpp"
#include "nk/common/allocator.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/string.h"
#include "nk/common/string.hpp"
#include "nk/common/string_builder.h"
#include "nk/common/utils.h"
#include "nk/common/utils.hpp"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/ir_compile.h"
#include "nk/vm/value.h"
#include "nkl/lang/ast.h"
#include "parser.hpp"

namespace {

namespace fs = std::filesystem;

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

struct Scope {
    bool is_top_level;
    std::unordered_map<nkid, Decl> locals{};
};

static thread_local NklCompiler s_compiler;

} // namespace

struct NklCompiler_T {
    NkIrProg ir;
    NkAllocator *arena;

    std::string stdlib_dir{};
    std::string libc_name{};
    bool configured = false;

    std::stack<Scope> nonpersistent_scope_stack{};
    std::deque<Scope> persistent_scopes{};
    std::vector<Scope *> scopes{};

    std::unordered_map<NkIrFunct, Scope *> fn_scopes{};

    NkIrFunct cur_fn{};

    std::map<fs::path, NkIrFunct> imports{};

    std::stack<std::string> comptime_const_names{};
    std::vector<nkid> node_ids{};
    size_t fn_counter{};
};

namespace {

Scope &curScope(NklCompiler c) {
    assert(!c->scopes.empty() && "no current scope");
    return *c->scopes.back();
}

void pushScope(NklCompiler c) {
    NK_LOG_DBG("entering scope=%lu", c->scopes.size());
    auto scope = &c->nonpersistent_scope_stack.emplace(Scope{false});
    c->scopes.emplace_back(scope);
}

void pushPersistentScope(NklCompiler c) {
    NK_LOG_DBG("entering persistent scope=%lu", c->scopes.size());
    auto scope = &c->persistent_scopes.emplace_back(Scope{false});
    c->scopes.emplace_back(scope);
}

void pushTopLevelScope(NklCompiler c) {
    NK_LOG_DBG("entering top level scope=%lu", c->scopes.size());
    auto scope = &c->persistent_scopes.emplace_back(Scope{true});
    c->scopes.emplace_back(scope);
}

void pushTopLevelFnScope(NklCompiler c, NkIrFunct fn) {
    pushTopLevelScope(c);
    c->fn_scopes.emplace(fn, &curScope(c));
}

void pushFnScope(NklCompiler c, NkIrFunct fn) {
    pushPersistentScope(c);
    c->fn_scopes.emplace(fn, &curScope(c));
}

void popScope(NklCompiler c) {
    assert(!c->scopes.empty() && "popping an empty scope stack");
    if (!c->nonpersistent_scope_stack.empty()) {
        auto scope = &c->nonpersistent_scope_stack.top();
        if (scope == c->scopes.back()) {
            c->nonpersistent_scope_stack.pop();
        }
    }
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

// TODO Restrict resolving local through stack frame boundaries
Decl &resolve(NklCompiler c, nkid name) {
    NK_LOG_DBG(
        "resolving name=`%.*s` scope=%lu",
        nkid2s(name).size,
        nkid2s(name).data,
        c->scopes.size() - 1);

    for (size_t i = c->scopes.size(); i > 0; i--) {
        auto &scope = *c->scopes[i - 1];
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

ValueInfo makeValue(nkval_t val) {
    return {{.val = nkval_data(val)}, val.type, v_val};
}

ValueInfo makeInstr(NkIrInstr const &instr, nktype_t type) {
    return {{.instr{instr}}, type, v_instr};
}

ValueInfo makeRef(NkIrRef const &ref) {
    return {{.ref = ref}, ref.type, v_ref};
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
    default:
        assert(!"unreachable");
        return {};
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
    default:
        assert(!"unreachable");
        return {};
    }
}

NkIrRef asRef(NklCompiler c, ValueInfo const &val) {
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

ValueInfo store(NklCompiler c, NkIrRef const &dst, ValueInfo val) {
    if (val.type->typeclass_id != NkType_Void) {
        if (val.kind == v_instr && val.as.instr.arg[0].ref.ref_type == NkIrRef_None) {
            val.as.instr.arg[0].ref = dst;
        } else {
            gen(c, nkir_make_mov(dst, asRef(c, val)));
            return makeRef(dst);
        }
    }
    return makeRef(asRef(c, val));
}

ValueInfo declToValueInfo(Decl &decl) {
    switch (decl.kind) {
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
        assert(!"unreachable");
        return {};
    }
}

[[nodiscard]] auto pushFn(NklCompiler c, NkIrFunct fn) {
    auto prev_fn = c->cur_fn;
    c->cur_fn = fn;
    return makeDeferrer([=]() {
        c->cur_fn = prev_fn;
        nkir_activateFunct(c->ir, c->cur_fn);
    });
}

ComptimeConst comptimeCompileNode(NklCompiler c, NklAstNode node);
nkval_t comptimeCompileNodeGetValue(NklCompiler c, NklAstNode node);
ValueInfo compileNode(NklCompiler c, NklAstNode node);
void compileStmt(NklCompiler c, NklAstNode node);
void compileNodeArray(NklCompiler c, NklAstNodeArray nodes);

struct IndexResult {
    size_t offset;
    nktype_t type;
};

// TODO IndexResult calcArrayIndex(nktype_t type, size_t index) {
//     size_t elem_count = type->as.arr.elem_count;
//     size_t elem_size = type->as.arr.elem_type->size;
//     if (index >= elem_count) {
//         NK_LOG_ERR("array index out of range");
//         std::abort();
//     }
//     return {elem_size * index, type->as.arr.elem_type};
// }

IndexResult calcTupleIndex(nktype_t type, size_t index) {
    size_t elem_count = type->as.tuple.elems.size;
    if (index >= elem_count) {
        NK_LOG_ERR("tuple index out of range");
        std::abort();
    }
    return {type->as.tuple.elems.data[index].offset, type->as.tuple.elems.data[index].type};
}

NkIrRef getLvalueRef(NklCompiler c, NklAstNode node) {
    NkIrRef ref{};
    if (node->id == cs2nkid("id")) {
        auto name = node->token->text;
        auto res = resolve(c, s2nkid(name));
        switch (res.kind) {
        case Decl_Local:
            ref = nkir_makeFrameRef(c->ir, res.as.local.id);
            break;
        case Decl_Global:
            ref = nkir_makeGlobalRef(c->ir, res.as.global.id);
            break;
        case Decl_Undefined:
            NK_LOG_ERR("`%.*s` is not defined", name.size, name.data);
            std::abort(); // TODO Report errors properly
        case Decl_Funct:
        case Decl_ExtSym:
        case Decl_Arg:
            NK_LOG_ERR("cannot assign to `%.*s`", name.size, name.data);
            std::abort();
        default:
            NK_LOG_ERR("unknown decl type");
            assert(!"unreachable");
            return {};
        }
    } else if (node->id == cs2nkid("index")) {
        ref = asRef(c, compileNode(c, node->args[0].data));
        auto type = ref.type;
        auto index = compileNode(c, node->args[1].data);
        if (index.type->typeclass_id != NkType_Numeric) {
            NK_LOG_ERR("expected number in index");
            std::abort();
        }
        // TODO Optimize array indexing
        if (type->typeclass_id == NkType_Array) {
            auto u64_t = nkt_get_numeric(c->arena, Uint64);
            ref = asRef(
                c,
                makeInstr(
                    nkir_make_add(
                        {},
                        asRef(c, makeInstr(nkir_make_lea({}, ref), u64_t)),
                        asRef(
                            c,
                            makeInstr(
                                nkir_make_mul(
                                    {},
                                    asRef(c, index),
                                    asRef(
                                        c,
                                        makeValue<uint64_t>(
                                            c, u64_t, type->as.arr.elem_type->size))),
                                u64_t))),
                    u64_t));
            ref.is_indirect = true;
        } else if (type->typeclass_id == NkType_Tuple) {
            if (!isKnown(index)) {
                NK_LOG_ERR("comptime value expected in tuple index");
                std::abort();
            }
            auto idx_res = calcTupleIndex(type, nkval_as(uint64_t, asValue(c, index)));
            ref.post_offset += idx_res.offset;
            ref.type = idx_res.type;
        } else {
            auto sb = nksb_create();
            defer {
                nksb_free(sb);
            };
            nkt_inspect(type, sb);
            auto type_str = nksb_concat(sb);
            NK_LOG_ERR("type `%.*s` is not indexable", type_str.size, type_str.data);
            std::abort();
        }
    } else if (node->id == cs2nkid("deref")) {
        auto arg = compileNode(c, node->args[0].data);
        if (arg.type->typeclass_id != NkType_Ptr) {
            NK_LOG_ERR("pointer expected in dereference");
            std::abort();
        }
        ref = asRef(c, arg);
        ref.is_indirect = true;
        ref.type = arg.type->as.ptr.target_type;
    } else {
        NK_LOG_ERR("invalid lvalue");
        std::abort();
    }
    return ref;
}

NkIrFunct nkl_compileFile(NklCompiler c, nkstr path);

#define COMPILE(NAME) ValueInfo _compile_##NAME(NklCompiler c, NklAstNode node)

COMPILE(none) {
    (void)c;
    (void)node;
    return {};
}

COMPILE(nop) {
    (void)c;
    (void)node;
    nkir_gen(c->ir, nkir_make_nop()); // TODO Do we actually need to generate nop?
    return makeVoid(c);
}

COMPILE(true) {
    (void)node;
    return makeValue<bool>(c, nkt_get_numeric(c->arena, Uint8), true);
}

COMPILE(false) {
    (void)node;
    return makeValue<bool>(c, nkt_get_numeric(c->arena, Uint8), false);
}

// TODO Modeling type_t as *void

COMPILE(i8) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Int8));
}

COMPILE(i16) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Int16));
}

COMPILE(i32) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Int32));
}

COMPILE(i64) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Int64));
}

COMPILE(u8) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Uint8));
}

COMPILE(u16) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Uint16));
}

COMPILE(u32) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Uint32));
}

COMPILE(u64) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Uint64));
}

COMPILE(f32) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Float32));
}

COMPILE(f64) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_numeric(c->arena, Float64));
}

COMPILE(bool) {
    (void)node;
    return makeValue<nktype_t>(
        c,
        nkt_get_ptr(c->arena, nkt_get_void(c->arena)),
        nkt_get_numeric(c->arena, Uint8)); // TODO Modeling bool as u8
}

COMPILE(void) {
    (void)node;
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_void(c->arena));
}

COMPILE(addr) {
    auto arg = compileNode(c, node->args[0].data);
    return makeInstr(nkir_make_lea({}, asRef(c, arg)), nkt_get_ptr(c->arena, arg.type));
}

COMPILE(deref) {
    return makeRef(getLvalueRef(c, node));
}

COMPILE(return ) {
    auto arg = compileNode(c, node->args[0].data);
    store(c, nkir_makeRetRef(c->ir), arg);
    gen(c, nkir_make_ret()); // TODO potentially generating ret twice
    return makeVoid(c);
}

COMPILE(ptr_type) {
    auto target_type = nkval_as(nktype_t, comptimeCompileNodeGetValue(c, node->args[0].data));
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_ptr(c->arena, target_type));
}

COMPILE(const_ptr_type) {
    // TODO Ignoring const in const_ptr_type
    auto target_type = nkval_as(nktype_t, comptimeCompileNodeGetValue(c, node->args[0].data));
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_ptr(c->arena, target_type));
}

COMPILE(scope) {
    NK_LOG_WRN("TODO scope implementation not finished");
    return compileNode(c, node->args[0].data);
}

COMPILE(run) {
    return makeValue(comptimeCompileNodeGetValue(c, node->args[0].data));
}

// TODO Not doing type checks in arithmetic

COMPILE(add) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_add({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(sub) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_sub({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(mul) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_mul({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(div) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_div({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(mod) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_mod({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(bitand) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_bitand({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(bitor) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_bitor({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(xor) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_xor({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(lsh) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_lsh({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(rsh) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_rsh({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(and) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_and({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(or) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_or({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(eq) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_eq({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(ge) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_ge({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(gt) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_gt({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(le) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_le({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(lt) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_lt({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(ne) {
    auto lhs = compileNode(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return makeInstr(nkir_make_ne({}, asRef(c, lhs), asRef(c, rhs)), lhs.type);
}

COMPILE(array_type) {
    // TODO Hardcoded array size type in array_type
    auto type = nkval_as(nktype_t, comptimeCompileNodeGetValue(c, node->args[0].data));
    auto size = nkval_as(int64_t, comptimeCompileNodeGetValue(c, node->args[1].data));
    return makeValue<nktype_t>(
        c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), nkt_get_array(c->arena, type, size));
}

COMPILE(while) {
    auto l_loop = nkir_makeBlock(c->ir);
    auto l_endloop = nkir_makeBlock(c->ir);
    nkir_startBlock(c->ir, l_loop, cs2s("loop"));
    gen(c, nkir_make_enter());
    auto cond = compileNode(c, node->args[0].data);
    gen(c, nkir_make_jmpz(asRef(c, cond), l_endloop));
    compileStmt(c, node->args[1].data);
    gen(c, nkir_make_leave());
    gen(c, nkir_make_jmp(l_loop));
    nkir_startBlock(c->ir, l_endloop, cs2s("endloop"));
    return makeVoid(c);
}

COMPILE(index) {
    return makeRef(getLvalueRef(c, node));
}

COMPILE(if) {
    auto l_endif = nkir_makeBlock(c->ir);
    NkIrBlockId l_else;
    if (node->args[2].data && node->args[2].data->id) {
        l_else = nkir_makeBlock(c->ir);
    } else {
        l_else = l_endif;
    }
    auto cond = compileNode(c, node->args[0].data);
    gen(c, nkir_make_jmpz(asRef(c, cond), l_else));
    {
        pushScope(c);
        defer {
            popScope(c);
        };
        compileStmt(c, node->args[1].data);
    }
    if (node->args[2].data && node->args[2].data->id) {
        gen(c, nkir_make_jmp(l_endif));
        nkir_startBlock(c->ir, l_else, cs2s("else"));
        {
            pushScope(c);
            defer {
                popScope(c);
            };
            compileStmt(c, node->args[2].data);
        }
    }
    nkir_startBlock(c->ir, l_endif, cs2s("endif"));
    return makeVoid(c);
}

COMPILE(block) {
    compileNodeArray(c, node->args[0]);
    return makeVoid(c);
}

COMPILE(tuple_type) {
    auto nodes = node->args[0];
    std::vector<nktype_t> types;
    types.reserve(nodes.size);
    for (size_t i = 0; i < nodes.size; i++) {
        types.emplace_back(nkval_as(nktype_t, comptimeCompileNodeGetValue(c, &nodes.data[i])));
    }
    return makeValue<nktype_t>(
        c,
        nkt_get_ptr(c->arena, nkt_get_void(c->arena)),
        nkt_get_tuple(c->arena, types.data(), types.size(), 1));
}

COMPILE(import) {
    NK_LOG_WRN("TODO import implementation not finished");
    if (!c->configured) {
        NK_LOG_ERR("stdlib is not available");
        std::abort();
    }
    auto name = node->args[0].data->token->text;
    std::string filename = std_str(name) + ".nkl";
    auto filepath = (fs::path{c->stdlib_dir} / filename).lexically_normal();
    auto it = c->imports.find(filepath);
    if (it == c->imports.end()) {
        auto filepath_str = filepath.string();
        if (!fs::exists(filepath)) {
            NK_LOG_ERR(
                "imported file `%.*s` doesn't exist", filepath_str.size(), filepath_str.c_str());
            std::abort();
        }
        auto fn = nkl_compileFile(c, {filepath_str.c_str(), filepath_str.size()});
        std::tie(it, std::ignore) = c->imports.emplace(filepath, fn);
    }
    auto fn = it->second;
    auto fn_val = asValue(c, makeValue<void *>(c, nkir_functGetType(fn), fn));
    defineComptimeConst(c, s2nkid(name), {{.value{fn_val}}, ComptimeConst_Value});
    return makeVoid(c);
}

COMPILE(id) {
    nkstr name_str = node->token->text;
    nkid name = s2nkid(name_str);
    auto &decl = resolve(c, name);
    if (decl.kind == Decl_Undefined) {
        NK_LOG_ERR("`%.*s` is not defined", name_str.size, name_str.data);
        std::abort(); // TODO Report errors properly
    } else {
        return declToValueInfo(decl);
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

COMPILE(escaped_string) {
    auto sb = nksb_create();
    defer {
        nksb_free(sb);
    };
    nksb_str_unescape(sb, node->token->text);
    auto unescaped_str = nksb_concat(sb);

    auto size = unescaped_str.size;

    auto u8_t = nkt_get_numeric(c->arena, Uint8);
    auto ar_t = nkt_get_array(c->arena, u8_t, size + 1);
    auto str_t = nkt_get_ptr(c->arena, ar_t);

    auto str = (char *)nk_allocate(c->arena, unescaped_str.size + 1);
    std::memcpy(str, unescaped_str.data, size);
    str[size] = '\0';

    return makeValue<char *>(c, str_t, str);
}

COMPILE(member) {
    NK_LOG_WRN("TODO member implementation not finished");
    auto lhs = compileNode(c, node->args[0].data);
    auto name = s2nkid(node->args[1].data->token->text);
    if (isKnown(lhs)) {
        auto lhs_val = asValue(c, lhs);
        if (nkval_typeclassid(lhs_val) == NkType_Fn &&
            nkval_typeof(lhs_val)->as.fn.call_conv == NkCallConv_Nk) {
            auto scope_it = c->fn_scopes.find(*(NkIrFunct *)nkval_data(lhs_val));
            if (scope_it != c->fn_scopes.end()) {
                auto &scope = *scope_it->second;
                auto member_it = scope.locals.find(name);
                if (member_it != scope.locals.end()) {
                    auto &decl = member_it->second;
                    return declToValueInfo(decl);
                } else {
                    NK_LOG_ERR("member `%s` not found", nkid2cs(name));
                    std::abort(); // TODO Report errors properly
                }
            }
        }
    }
    return makeVoid(c);
}

ValueInfo compileFn(NklCompiler c, NklAstNode node, bool is_variadic) {
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

    nktype_t ret_t = node->args[1].data
                         ? nkval_as(nktype_t, comptimeCompileNodeGetValue(c, node->args[1].data))
                         : nkt_get_void(c->arena);
    nktype_t args_t = nkt_get_tuple(c->arena, params_types.data(), params_types.size(), 1);

    auto fn_t = nkt_get_fn(c->arena, {ret_t, args_t, NkCallConv_Nk, is_variadic});

    auto fn = nkir_makeFunct(c->ir);
    auto pop_fn = pushFn(c, fn);

    std::string fn_name;
    if (c->node_ids.size() >= 2 && *(c->node_ids.rbegin() + 1) == n_comptime_const_def) {
        fn_name = c->comptime_const_names.top();
    } else {
        fn_name.resize(100);
        int n = std::snprintf(fn_name.data(), fn_name.size(), "fn%zu", c->fn_counter++);
        fn_name.resize(n);
    }
    nkir_startFunct(fn, {fn_name.c_str(), fn_name.size()}, fn_t);
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), cs2s("start"));

    pushFnScope(c, fn);
    defer {
        popScope(c);
    };

    for (size_t i = 0; i < params.size; i++) {
        defineArg(c, params_names[i], i, params_types[i]);
    }

    compileStmt(c, node->args[2].data);
    gen(c, nkir_make_ret());

    NK_LOG_INF(
        "ir:\n%s", (char const *)[&]() {
            auto sb = nksb_create();
            nkir_inspectFunct(fn, sb);
            return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                nksb_free(sb);
            });
        }());

    return makeValue<void *>(c, fn_t, fn);
}

COMPILE(fn) {
    return compileFn(c, node, false);
}

COMPILE(fn_var) {
    return compileFn(c, node, true);
}

ValueInfo compileFnType(NklCompiler c, NklAstNode node, bool is_variadic) {
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

    auto fn_t = nkt_get_fn(
        c->arena,
        {ret_t, args_t, NkCallConv_Cdecl, is_variadic}); // TODO CallConv Hack for #foreign

    return makeValue<nktype_t>(c, nkt_get_ptr(c->arena, nkt_get_void(c->arena)), fn_t);
}

COMPILE(fn_type) {
    return compileFnType(c, node, false);
}

COMPILE(fn_type_var) {
    return compileFnType(c, node, true);
}

COMPILE(tag) {
    NK_LOG_WRN("TODO Only handling #foreign tag");

    auto n_tag_name = node->args[0];
    auto n_args = node->args[1];
    auto n_def = node->args[2];

    assert(
        0 == std::strncmp(
                 n_tag_name.data->token->text.data, "#foreign", n_tag_name.data->token->text.size));

    assert(n_args.size == 1 || n_args.size == 2);
    std::string soname =
        nkval_as(char const *, comptimeCompileNodeGetValue(c, n_args.data[0].args[1].data));
    if (soname == "c" || soname == "C") {
        soname = c->libc_name;
    }
    std::string link_prefix =
        n_args.size == 2
            ? nkval_as(char const *, comptimeCompileNodeGetValue(c, n_args.data[1].args[1].data))
            : "";

    assert(n_def.data->id == n_comptime_const_def);

    auto so = nkir_makeShObj(c->ir, cs2s(soname.c_str())); // TODO Creating so every time

    auto sym_name = n_def.data->args[0].data->token->text;
    auto sym_name_with_prefix_std_str = link_prefix + std_str(sym_name);
    nkstr sym_name_with_prefix{
        sym_name_with_prefix_std_str.data(), sym_name_with_prefix_std_str.size()};

    auto fn_t = nkval_as(nktype_t, comptimeCompileNodeGetValue(c, n_def.data->args[1].data));

    defineExtSym(c, s2nkid(sym_name), nkir_makeExtSym(c->ir, so, sym_name_with_prefix, fn_t), fn_t);

    return makeVoid(c);
}

COMPILE(call) {
    auto lhs = compileNode(c, node->args[0].data);

    std::vector<ValueInfo> args_info{};
    std::vector<nktype_t> args_types{};

    auto nodes = node->args[1];
    for (size_t i = 0; i < nodes.size; i++) {
        auto val_info =
            compileNode(c, nodes.data[i].args[1].data); // TODO Support named args in call
        args_info.emplace_back(val_info);
        args_types.emplace_back(val_info.type);
    }

    auto args_t = nkt_get_tuple(c->arena, args_types.data(), args_types.size(), 1);

    NkIrRef args{};
    if (args_t->size) {
        args = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, args_t));
    }

    auto fn_t = lhs.type;

    for (size_t i = 0; i < args_info.size(); i++) {
        auto arg_ref = args;
        arg_ref.offset += args_t->as.tuple.elems.data[i].offset;
        arg_ref.type = args_t->as.tuple.elems.data[i].type;
        store(c, arg_ref, args_info[i]);
    }

    return makeInstr(nkir_make_call({}, asRef(c, lhs), args), fn_t->as.fn.ret_t);
}

void initFromAst(NklCompiler c, nkval_t val, NklAstNodeArray init_nodes) {
    switch (nkval_typeclassid(val)) {
    case NkType_Array:
        if (init_nodes.size > nkval_tuple_size(val)) {
            NK_LOG_ERR("too many values to init array");
            std::abort();
        }
        for (size_t i = 0; i < nkval_array_size(val); i++) {
            initFromAst(c, nkval_array_at(val, i), {&init_nodes.data[i], 1});
        }
        break;
    case NkType_Numeric: {
        if (init_nodes.size > 1) {
            NK_LOG_ERR("too many values to init numeric");
            std::abort();
        }
        auto const &node = init_nodes.data[0];
        if (node.id != cs2nkid("int") && node.id != cs2nkid("float")) {
            NK_LOG_ERR("invalid value to init numeric");
            std::abort();
        }
        auto str = std_str(node.token->text);
        switch (nkval_typeof(val)->as.num.value_type) {
        case Int8:
            nkval_as(int8_t, val) = std::stoll(str);
            break;
        case Int16:
            nkval_as(int16_t, val) = std::stoll(str);
            break;
        case Int32:
            nkval_as(int32_t, val) = std::stoll(str);
            break;
        case Int64:
            nkval_as(int64_t, val) = std::stoll(str);
            break;
        case Uint8:
            nkval_as(uint8_t, val) = std::stoull(str);
            break;
        case Uint16:
            nkval_as(uint16_t, val) = std::stoull(str);
            break;
        case Uint32:
            nkval_as(uint32_t, val) = std::stoull(str);
            break;
        case Uint64:
            nkval_as(uint64_t, val) = std::stoull(str);
            break;
        case Float32:
            nkval_as(float, val) = std::stof(str);
            break;
        case Float64:
            nkval_as(double, val) = std::stod(str);
            break;
        }
        break;
    }
    case NkType_Tuple:
        if (init_nodes.size > nkval_tuple_size(val)) {
            NK_LOG_ERR("too many values to init array");
            std::abort();
        }
        for (size_t i = 0; i < nkval_tuple_size(val); i++) {
            initFromAst(c, nkval_tuple_at(val, i), {&init_nodes.data[i], 1});
        }
        break;
    default:
        NK_LOG_ERR("initFromAst is not implemented fir this type");
        assert(!"unreachable");
        break;
    }
}

COMPILE(object_literal) {
    // TODO Optimize object literal

    auto type_val = comptimeCompileNodeGetValue(c, node->args[0].data);

    // TODO Modeling type_t as *void
    if (nkval_typeclassid(type_val) != NkType_Ptr &&
        nkval_typeof(type_val)->as.ptr.target_type->typeclass_id != NkType_Void) {
        NK_LOG_ERR("type expected in object literal");
        std::abort();
    }

    auto type = nkval_as(nktype_t, type_val);

    nkval_t val{nk_allocate(c->arena, type->size), type};
    std::memset(nkval_data(val), 0, nkval_sizeof(val));

    auto init_nodes = node->args[1];

    // TODO Ingnoreing named args in object literal, and copying values to a sperate array
    std::vector<NklAstNode_T> nodes;
    nodes.reserve(init_nodes.size);

    for (size_t i = 0; i < init_nodes.size; i++) {
        nodes.emplace_back(init_nodes.data[i].args[1].data[0]);
    }

    initFromAst(c, val, {nodes.data(), nodes.size()});

    return makeValue(val);
}

COMPILE(assign) {
    if (node->args->size > 1) {
        NK_LOG_WRN("TODO multiple assignment is not supported");
    }
    auto lhs_ref = getLvalueRef(c, node->args[0].data);
    auto rhs = compileNode(c, node->args[1].data);
    return store(c, lhs_ref, rhs);
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
    if (curScope(c).is_top_level) {
        auto var = nkir_makeGlobalVar(c->ir, rhs.type);
        defineGlobal(c, name, var, rhs.type);
        ref = nkir_makeGlobalRef(c->ir, var);
    } else {
        auto var = nkir_makeLocalVar(c->ir, rhs.type);
        defineLocal(c, name, var, rhs.type);
        ref = nkir_makeFrameRef(c->ir, var);
    }
    store(c, ref, rhs);
    return makeVoid(c);
}

COMPILE(comptime_const_def) {
    auto names = node->args[0];
    if (names.size > 1) {
        NK_LOG_ERR("multiple assignment is not implemented");
        std::abort(); // TODO Report errors properly
    }
    auto name_str = std_str(names.data[0].token->text);
    c->comptime_const_names.push(name_str);
    defer {
        c->comptime_const_names.pop();
    };
    auto decl = comptimeCompileNode(c, node->args[1].data);
    nkid name = s2nkid({name_str.data(), name_str.size()});
    defineComptimeConst(c, name, decl);
    return makeVoid(c);
}

COMPILE(var_decl) {
    auto const &names = node->args[0];
    if (names.size > 1) {
        NK_LOG_ERR("multiple assignment is not implemented");
        std::abort(); // TODO Report errors properly
    }
    nkid name = s2nkid(names.data[0].token->text);
    auto type = nkval_as(nktype_t, comptimeCompileNodeGetValue(c, node->args[1].data));
    NkIrRef ref;
    if (curScope(c).is_top_level) {
        auto var = nkir_makeGlobalVar(c->ir, type);
        defineGlobal(c, name, var, type);
        ref = nkir_makeGlobalRef(c->ir, var);
    } else {
        auto var = nkir_makeLocalVar(c->ir, type);
        defineLocal(c, name, var, type);
        ref = nkir_makeFrameRef(c->ir, var);
    }
    if (node->args[2].data && node->args[2].data->id) {
        auto val = compileNode(c, node->args[2].data);
        store(c, ref, val);
    }
    return makeVoid(c);
}

using CompileFunc = ValueInfo (*)(NklCompiler c, NklAstNode node);

CompileFunc s_funcs[] = {
#define X(NAME) CAT(_compile_, NAME),
#include "nodes.inl"
};

ValueInfo compileNode(NklCompiler c, NklAstNode node) {
    assert(node->id < AR_SIZE(s_funcs) && "invalid node");
#ifdef BUILD_WITH_EASY_PROFILER
    auto block_name = std::string{"compile: "} + s_nkl_ast_node_names[node->id];
#endif // BUILD_WITH_EASY_PROFILER
    EASY_BLOCK(block_name.c_str(), ::profiler::colors::DeepPurple100);
    NK_LOG_DBG("node: %s", s_nkl_ast_node_names[node->id]);
    c->node_ids.emplace_back(node->id);
    defer {
        c->node_ids.pop_back();
    };
    return s_funcs[node->id](c, node);
}

ComptimeConst comptimeCompileNode(NklCompiler c, NklAstNode node) {
    auto fn = nkir_makeFunct(c->ir);
    NktFnInfo fn_info{nullptr, nkt_get_tuple(c->arena, nullptr, 0, 1), NkCallConv_Nk, false};

    auto pop_fn = pushFn(c, fn);

    nkir_startIncompleteFunct(fn, cs2s("#comptime"), &fn_info);
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), cs2s("start"));

    pushFnScope(c, fn);
    defer {
        popScope(c);
    };

    auto cnst_val = compileNode(c, node);
    nkir_incompleteFunctGetInfo(fn)->ret_t = cnst_val.type;
    nkir_finalizeIncompleteFunct(fn, c->arena);

    ComptimeConst cnst{};

    if (isKnown(cnst_val)) {
        nkir_discardFunct(fn);
        c->fn_scopes.erase(fn); // TODO Actually delete persistent scopes

        cnst.value = asValue(c, cnst_val);
        cnst.kind = ComptimeConst_Value;
    } else {
        store(c, nkir_makeRetRef(c->ir), cnst_val);
        gen(c, nkir_make_ret());

        cnst.funct = fn;
        cnst.kind = ComptimeConst_Funct;

        NK_LOG_INF(
            "ir:\n%s", (char const *)[&]() {
                auto sb = nksb_create();
                nkir_inspectFunct(fn, sb);
                return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                    nksb_free(sb);
                });
            }());
    }

    return cnst;
}

nkval_t comptimeCompileNodeGetValue(NklCompiler c, NklAstNode node) {
    auto cnst = comptimeCompileNode(c, node);
    return comptimeConstGetValue(c, cnst);
}

void compileStmt(NklCompiler c, NklAstNode node) {
    auto val = compileNode(c, node);
    auto ref = asRef(c, val);
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

NkIrFunct nkl_compile(NklCompiler c, NklAstNode root) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);

    auto fn = nkir_makeFunct(c->ir);
    auto pop_fn = pushFn(c, fn);

    auto top_level_fn_t = nkt_get_fn(
        c->arena,
        {nkt_get_void(c->arena), nkt_get_tuple(c->arena, nullptr, 0, 1), NkCallConv_Nk, false});

    nkir_startFunct(fn, cs2s("#top_level"), top_level_fn_t);
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), cs2s("start"));

    pushTopLevelFnScope(c, fn);
    defer {
        popScope(c);
    };

    if (root) {
        c->cur_fn = fn;
        compileStmt(c, root);
    }

    gen(c, nkir_make_ret());

    NK_LOG_INF(
        "ir:\n%s", (char const *)[&]() {
            auto sb = nksb_create();
            nkir_inspectFunct(fn, sb);
            nkir_inspectExtSyms(c->ir, sb);
            return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                nksb_free(sb);
            });
        }());

    return fn;
}

NkIrFunct nkl_compileSrc(NklCompiler c, nkstr src) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);
    auto tokens = nkl_lex(src);
    auto ast = nkl_ast_create();
    defer {
        nkl_ast_free(ast);
    };
    auto root = nkl_parse(ast, tokens);
    return nkl_compile(c, root);
}

NkIrFunct nkl_compileFile(NklCompiler c, nkstr path) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);
    std::ifstream file{std_str(path)};
    if (file) {
        std::string src{std::istreambuf_iterator<char>{file}, {}};
        return nkl_compileSrc(c, {src.data(), src.size()});
    } else {
        NK_LOG_ERR("failed to open file `%.*s`", path.size, path.data);
        std::abort();
    }
}

} // namespace

extern "C" NK_EXPORT void nkl_compiler_declareLocal(char const *name, nktype_t type) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);
    NklCompiler c = s_compiler;
    defineLocal(c, cs2nkid(name), nkir_makeLocalVar(c->ir, type), type);
}

extern "C" NK_EXPORT void nkl_compiler_buildExecutable(
    NkIrFunct entry_point,
    char const *exe_name) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);

    NklCompiler c = s_compiler;

    // TODO Hardcoded compiler settings
    NkIrCompilerConfig conf{
        .compiler_binary = cs2s("gcc"),
        .additional_flags = cs2s("-O2"),
        .output_filename = cs2s(exe_name),
        .quiet = 1,
    };
    nkir_compile(conf, c->ir, entry_point);
}

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

template <class T>
T getConfigValue(NklCompiler c, std::string const &name, decltype(Scope::locals) &config) {
    auto it = config.find(cs2nkid(name.c_str()));
    if (it == config.end()) {
        NK_LOG_ERR("`%.*s` is missing in config", name.size(), name.data());
        std::abort();
    }
    auto val_info = declToValueInfo(it->second);
    if (!isKnown(val_info)) {
        NK_LOG_ERR("`%.*s` value is not known");
        std::abort();
    }
    auto val = asValue(c, val_info);
    return nkval_as(T, val); // TODO Reinterpret cast in compiler without check
}

void nkl_compiler_configure(NklCompiler c, nkstr config_dir) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);
    NK_LOG_DBG("config_dir=`%.*s`", config_dir.size, config_dir.data);
    auto config_filepath = fs::path{std_view(config_dir)} / "config.nkl";
    auto filepath_str = config_filepath.string();
    if (!fs::exists(config_filepath)) {
        NK_LOG_ERR("config file `%.*s` doesn't exist", filepath_str.size(), filepath_str.c_str());
        std::abort();
    }
    auto fn = nkl_compileFile(c, {filepath_str.c_str(), filepath_str.size()});
    auto &config = c->fn_scopes[fn]->locals;

    c->stdlib_dir = getConfigValue<char const *>(c, "stdlib_dir", config);
    NK_LOG_DBG("stdlib_dir=`%.*s`", c->stdlib_dir.size(), c->stdlib_dir.c_str());

    c->libc_name = getConfigValue<char const *>(c, "libc_name", config);
    NK_LOG_DBG("libc_name=`%.*s`", c->libc_name.size(), c->libc_name.c_str());

    c->configured = true;
}

void nkl_compiler_run(NklCompiler c, NklAstNode root) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);

    // TODO Boilerplate in nkl_compiler_run_*

    auto prev_compiler = s_compiler;
    s_compiler = c;
    defer {
        s_compiler = prev_compiler;
    };

    auto fn = nkl_compile(c, root);
    nkir_invoke({&fn, nkir_functGetType(fn)}, {}, {});
}

void nkl_compiler_runSrc(NklCompiler c, nkstr src) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);

    auto prev_compiler = s_compiler;
    s_compiler = c;
    defer {
        s_compiler = prev_compiler;
    };

    auto fn = nkl_compileSrc(c, src);
    nkir_invoke({&fn, nkir_functGetType(fn)}, {}, {});
}

void nkl_compiler_runFile(NklCompiler c, nkstr path) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);

    auto prev_compiler = s_compiler;
    s_compiler = c;
    defer {
        s_compiler = prev_compiler;
    };

    auto fn = nkl_compileFile(c, path);
    nkir_invoke({&fn, nkir_functGetType(fn)}, {}, {});
}
