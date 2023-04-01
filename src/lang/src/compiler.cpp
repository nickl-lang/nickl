#include "nkl/lang/compiler.h"

#include <algorithm>
#include <cmath>
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
#include "nk/common/common.h"
#include "nk/common/id.h"
#include "nk/common/logger.h"
#include "nk/common/profiler.hpp"
#include "nk/common/string.h"
#include "nk/common/string.hpp"
#include "nk/common/string_builder.h"
#include "nk/common/utils.h"
#include "nk/common/utils.hpp"
#include "nk/sys/term.h"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/ir_compile.h"
#include "nk/vm/value.h"
#include "nkl/lang/ast.h"
#include "nkl/lang/token.h"
#include "nkl/lang/value.h"
#include "parser.hpp"

#define CHECK(EXPR)              \
    EXPR;                        \
    do {                         \
        if (c->error_occurred) { \
            return {};           \
        }                        \
    } while (0)

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))
#define ASSIGN(SLOT, VAL) CHECK(SLOT = (VAL))
#define APPEND(AR, VAL) CHECK(AR.emplace_back(VAL))

namespace {

nkltype_t u8_t;
nkltype_t u16_t;
nkltype_t u32_t;
nkltype_t u64_t;
nkltype_t i8_t;
nkltype_t i16_t;
nkltype_t i32_t;
nkltype_t i64_t;
nkltype_t f32_t;
nkltype_t f64_t;

nkltype_t typeref_t;
nkltype_t void_t;
nkltype_t bool_t;
nkltype_t u8_ptr_t;

struct Void {};

namespace fs = std::filesystem;

NK_LOG_USE_SCOPE(compiler);

enum EDeclKind {
    Decl_Undefined,

    Decl_ComptimeConst,
    Decl_Local,
    Decl_Global,
    Decl_ExtSym,
    Decl_Arg,
};

enum EComptimeConstKind {
    ComptimeConst_Value,
    ComptimeConst_Funct,
};

struct ComptimeConst {
    union {
        nklval_t value;
        NkIrFunct funct;
    };
    EComptimeConstKind kind;
};

ComptimeConst makeValueComptimeConst(nklval_t val) {
    return {.value{val}, .kind = ComptimeConst_Value};
}

ComptimeConst makeFunctComptimeConst(NkIrFunct funct) {
    return {.funct = funct, .kind = ComptimeConst_Funct};
}

struct Decl { // TODO Compact the Decl struct
    union {
        ComptimeConst comptime_const;
        struct {
            NkIrLocalVarId id;
            nkltype_t type;
        } local;
        struct {
            NkIrGlobalVarId id;
            nkltype_t type;
        } global;
        struct {
            NkIrFunct id;
            nkltype_t fn_t;
        } funct;
        struct {
            NkIrExtSymId id;
            nkltype_t type;
        } ext_sym;
        struct {
            size_t index;
            nkltype_t type;
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
        NkIrConstId cnst;
        NkIrRef ref;
        NkIrInstr instr;
        Decl *decl;
    } as;
    nkltype_t type;
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
    NkAllocator arena;

    std::string err_str{};
    NklTokenRef err_token{};
    bool error_occurred = false;

    std::string compiler_dir{};
    std::string stdlib_dir{};
    std::string libc_name{};
    std::string libm_name{};
    std::string c_compiler{};
    std::string c_compiler_flags{};

    std::stack<Scope> nonpersistent_scope_stack{};
    std::deque<Scope> persistent_scopes{};
    std::vector<Scope *> scopes{};

    std::unordered_map<NkIrFunct, Scope *> fn_scopes{};

    NkIrFunct cur_fn{};

    std::map<fs::path, NkIrFunct> imports{};

    std::stack<std::string> comptime_const_names{};
    std::vector<NklAstNode> node_stack{};
    size_t fn_counter{};

    std::unordered_map<nkid, size_t> id2blocknum{};

    std::stack<fs::path> file_stack{};
    std::stack<nkstr> src_stack{};
};

namespace {

void error(NklCompiler c, char const *fmt, ...) {
    assert(!c->error_occurred && "compiler error already initialized");

    va_list ap;
    va_start(ap, fmt);
    c->err_str = string_vformat(fmt, ap);
    va_end(ap);

    if (!c->node_stack.empty()) {
        c->err_token = c->node_stack.back()->token;
    }

    c->error_occurred = true;
}

nkstr irBlockName(NklCompiler c, char const *name) {
    // TODO Not ideal string management in compiler
    auto id = cs2nkid(name);
    auto num = c->id2blocknum[id]++;
    char buf[1024];
    size_t size = std::snprintf(buf, AR_SIZE(buf), "%s%zu", name, num);

    auto str = (char *)nk_allocate(c->arena, size + 1);
    std::memcpy(str, buf, size);
    str[size] = '\0';

    return {str, size};
}

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

ValueInfo makeVoid() {
    return {{}, void_t, v_none};
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
        static Decl s_dummy_decl{};
        return error(c, "`%.*s` is already defined", name_str.size, name_str.data), s_dummy_decl;
    } else {
        std::tie(it, std::ignore) = locals.emplace(name, Decl{});
    }

    return it->second;
}

void defineComptimeConst(NklCompiler c, nkid name, ComptimeConst cnst) {
    NK_LOG_DBG("defining comptime const `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.comptime_const{cnst}}, Decl_ComptimeConst};
}

void defineLocal(NklCompiler c, nkid name, NkIrLocalVarId id, nkltype_t type) {
    NK_LOG_DBG("defining local `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.local{id, type}}, Decl_Local};
}

void defineGlobal(NklCompiler c, nkid name, NkIrGlobalVarId id, nkltype_t type) {
    NK_LOG_DBG("defining global `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.global{id, type}}, Decl_Global};
}

void defineExtSym(NklCompiler c, nkid name, NkIrExtSymId id, nkltype_t type) {
    NK_LOG_DBG("defining ext sym `%.*s`", nkid2s(name).size, nkid2s(name).data);
    makeDecl(c, name) = {{.ext_sym{.id = id, .type = type}}, Decl_ExtSym};
}

void defineArg(NklCompiler c, nkid name, size_t index, nkltype_t type) {
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
ValueInfo makeValue(NklCompiler c, nkltype_t type, TArgs &&...args) {
    return {
        {.cnst = nkir_makeConst(
             c->ir, {new (nk_allocate(c->arena, sizeof(T))) T{args...}, tovmt(type)})},
        type,
        v_val};
}

ValueInfo makeValue(NklCompiler c, nklval_t val) {
    return {{.cnst = nkir_makeConst(c->ir, tovmv(val))}, nklval_typeof(val), v_val};
}

ValueInfo makeInstr(NkIrInstr const &instr, nkltype_t type) {
    return {{.instr{instr}}, type, v_instr};
}

ValueInfo makeRef(NkIrRef const &ref) {
    return {{.ref = ref}, fromvmt(ref.type), v_ref};
}

nklval_t comptimeConstGetValue(NklCompiler c, ComptimeConst &cnst) {
    switch (cnst.kind) {
    case ComptimeConst_Value:
        NK_LOG_DBG("returning comptime const as value");
        return cnst.value;
    case ComptimeConst_Funct: {
        NK_LOG_DBG("getting comptime const from funct");
        auto fn_t = nkir_functGetType(cnst.funct);
        auto type = fromvmt(fn_t->as.fn.ret_t);
        nklval_t val{nk_allocate(c->arena, nklt_sizeof(type)), type};
        nkir_invoke({&cnst.funct, fn_t}, tovmv(val), {});
        cnst = makeValueComptimeConst(val);
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

nklval_t asValue(NklCompiler c, ValueInfo const &val) {
    assert(isKnown(val) && "getting unknown value");
    if (val.kind == v_val) {
        return nklval_reinterpret_cast(val.type, fromvmv(nkir_constGetValue(c->ir, val.as.cnst)));
    } else {
        return comptimeConstGetValue(c, val.as.decl->as.comptime_const);
    }
}

nkltype_t comptimeConstType(ComptimeConst cnst) {
    switch (cnst.kind) {
    case ComptimeConst_Value:
        return nklval_typeof(cnst.value);
    case ComptimeConst_Funct:
        return fromvmt(nkir_functGetType(cnst.funct)->as.fn.ret_t);
    default:
        assert(!"unreachable");
        return {};
    }
}

NkIrRef asRef(NklCompiler c, ValueInfo const &val) {
    switch (val.kind) {
    case v_val: {
        auto ref = nkir_makeConstRef(c->ir, val.as.cnst);
        ref.type = tovmt(val.type);
        return ref;
    }

    case v_ref:
        return val.as.ref;

    case v_instr: {
        auto instr = val.as.instr;
        auto &dst = instr.arg[0].ref;
        if (dst.ref_type == NkIrRef_None && val.type->tclass != NkType_Void) {
            dst = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(val.type)));
        }
        gen(c, instr);
        return dst;
    }

    case v_decl: {
        auto &decl = *val.as.decl;
        switch (decl.kind) {
        case Decl_ComptimeConst:
            return nkir_makeConstRef(
                c->ir,
                nkir_makeConst(c->ir, tovmv(comptimeConstGetValue(c, decl.as.comptime_const))));
        case Decl_Local:
            return nkir_makeFrameRef(c->ir, decl.as.local.id);
        case Decl_Global:
            return nkir_makeGlobalRef(c->ir, decl.as.global.id);
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

ValueInfo store(NklCompiler c, NkIrRef const &dst, ValueInfo src) {
    auto const dst_type = fromvmt(dst.type);
    auto const src_type = src.type;
    if (nklt_tclass(dst_type) == NklType_Slice && nklt_tclass(src_type) == NkType_Ptr &&
        nklt_tclass(src_type->as.ptr.target_type) == NkType_Array &&
        nklt_typeid(dst_type->as.slice.target_type) ==
            nklt_typeid(src_type->as.ptr.target_type->as.arr.elem_type)) {
        auto const elem_ptr_t = nkl_get_ptr(dst_type->as.slice.target_type);
        auto ptr_ref = dst;
        ptr_ref.type = tovmt(elem_ptr_t);
        auto size_ref = dst;
        size_ref.type = tovmt(u64_t);
        size_ref.post_offset += nklt_sizeof(elem_ptr_t);
        auto src_ptr = src;
        src_ptr.type = elem_ptr_t;
        store(c, ptr_ref, src_ptr);
        store(
            c,
            size_ref,
            makeValue<uint64_t>(c, u64_t, src_type->as.ptr.target_type->as.arr.elem_count));
        return makeRef(dst);
    }
    if (nklt_typeid(dst_type) != nklt_typeid(src_type) &&
        !(nklt_tclass(dst_type) == NkType_Ptr && nklt_tclass(src_type) == NkType_Ptr &&
          nklt_tclass(src_type->as.ptr.target_type) == NkType_Array &&
          nklt_typeid(src_type->as.ptr.target_type->as.arr.elem_type) ==
              nklt_typeid(dst_type->as.ptr.target_type))) {
        // TODO A little clumsy error printing in store
        auto dst_sb = nksb_create();
        auto src_sb = nksb_create();
        defer {
            nksb_free(dst_sb);
            nksb_free(src_sb);
        };
        nklt_inspect(dst_type, dst_sb);
        nklt_inspect(src_type, src_sb);
        auto dst_type_str = nksb_concat(dst_sb);
        auto src_type_str = nksb_concat(src_sb);
        return error(
                   c,
                   "cannot store value of type `%.*s` into a slot of type `%.*s`",
                   src_type_str.size,
                   src_type_str.data,
                   dst_type_str.size,
                   dst_type_str.data),
               ValueInfo{};
    }
    if (src_type->tclass != NkType_Void) {
        if (src.kind == v_instr && src.as.instr.arg[0].ref.ref_type == NkIrRef_None) {
            src.as.instr.arg[0].ref = dst;
        } else {
            src = makeInstr(nkir_make_mov(dst, asRef(c, src)), dst_type);
        }
    }
    return makeRef(asRef(c, src));
}

ValueInfo declToValueInfo(Decl &decl) {
    switch (decl.kind) {
    case Decl_ComptimeConst:
        return {{.decl = &decl}, comptimeConstType(decl.as.comptime_const), v_decl};
    case Decl_Local:
        return {{.decl = &decl}, decl.as.local.type, v_decl};
    case Decl_Global:
        return {{.decl = &decl}, decl.as.global.type, v_decl};
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
nklval_t comptimeCompileNodeGetValue(NklCompiler c, NklAstNode node);
ValueInfo compile(NklCompiler c, NklAstNode node);
Void compileStmt(NklCompiler c, NklAstNode node);

struct IndexResult {
    size_t offset;
    nkltype_t type;
};

// TODO IndexResult calcArrayIndex(NklCompiler c, nkltype_t type, size_t index) {
//     size_t elem_count = type->as.arr.elem_count;
//     size_t elem_size = type->as.arr.elem_type->size;
//     if (index >= elem_count) {
//         return error(c, "array index out of range"), IndexResult{};
//     }
//     return {elem_size * index, type->as.arr.elem_type};
// }

IndexResult calcTupleIndex(NklCompiler c, nkltype_t type, size_t index) {
    size_t elem_count = type->as.tuple.elems.size;
    if (index >= elem_count) {
        return error(c, "tuple index out of range"), IndexResult{};
    }
    return {type->as.tuple.elems.data[index].offset, type->as.tuple.elems.data[index].type};
}

ValueInfo compileStructIndex(
    NklCompiler c,
    ValueInfo const &lhs,
    nkltype_t struct_type,
    nkid name) {
    auto index = nklt_struct_index(struct_type, name);
    if (index == -1ull) {
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };
        nklt_inspect(lhs.type, sb);
        auto type_str = nksb_concat(sb);
        return error(
                   c,
                   "no field named `%s` in type `%.*s`",
                   nkid2cs(name),
                   type_str.size,
                   type_str.data),
               ValueInfo{};
    }
    auto ref = asRef(c, lhs);
    // TODO Probably need a better way of accessing struct info
    ref.type = tovmt(struct_type->as.strct.fields.data[index].type);
    ref.post_offset += tovmt(struct_type)->as.tuple.elems.data[index].offset;
    return makeRef(ref);
}

ValueInfo getLvalueRef(NklCompiler c, NklAstNode node) {
    if (node->id == n_id) {
        auto const name = node->token->text;
        auto const res = resolve(c, s2nkid(name));
        switch (res.kind) {
        case Decl_Local:
            return makeRef(nkir_makeFrameRef(c->ir, res.as.local.id));
        case Decl_Global:
            return makeRef(nkir_makeGlobalRef(c->ir, res.as.global.id));
        case Decl_Undefined:
            return error(c, "`%.*s` is not defined", name.size, name.data), ValueInfo{};
        case Decl_ExtSym:
        case Decl_Arg:
            return error(c, "cannot assign to `%.*s`", name.size, name.data), ValueInfo{};
        default:
            NK_LOG_ERR("unknown decl type");
            assert(!"unreachable");
            return {};
        }
    } else if (node->id == n_index) {
        DEFINE(lhs, compile(c, narg0(node)));
        auto const lhs_ref = asRef(c, lhs);
        auto const type = fromvmt(lhs_ref.type);
        DEFINE(index, compile(c, narg1(node)));
        if (index.type->tclass != NkType_Numeric) {
            return error(c, "expected number in index"), ValueInfo{};
        }
        // TODO Optimize array indexing
        // TODO Think about type correctness in array indexing
        if (type->tclass == NkType_Array) {
            // TODO using u8 to correctly index array in c
            auto const elem_t = type->as.arr.elem_type;
            auto const elem_ptr_t = nkl_get_ptr(elem_t);
            auto const data_ref = asRef(c, makeInstr(nkir_make_lea({}, lhs_ref), u8_ptr_t));
            auto const mul = nkir_make_mul(
                {},
                asRef(c, index),
                asRef(c, makeValue<uint64_t>(c, u64_t, nklt_sizeof(type->as.arr.elem_type))));
            auto const offset_ref = asRef(c, makeInstr(mul, u64_t));
            auto const add = nkir_make_add({}, data_ref, offset_ref);
            auto ref = asRef(c, makeInstr(add, elem_ptr_t));
            ref.is_indirect = true;
            ref.type = tovmt(elem_t);
            return makeRef(ref);
        } else if (type->tclass == NkType_Tuple) {
            if (!isKnown(index)) {
                return error(c, "comptime value expected in tuple index"), ValueInfo{};
            }
            DEFINE(idx_res, calcTupleIndex(c, type, nklval_as(uint64_t, asValue(c, index))));
            auto ref = lhs_ref;
            ref.post_offset += idx_res.offset;
            ref.type = tovmt(idx_res.type);
            return makeRef(ref);
        } else if (type->tclass == NklType_Slice) {
            // TODO Using u8 to correctly index slice in c
            auto const elem_t = type->as.slice.target_type;
            auto const elem_ptr_t = nkl_get_ptr(type->as.slice.target_type);
            auto data_ref = lhs_ref;
            data_ref.type = tovmt(u8_ptr_t);
            auto const mul = nkir_make_mul(
                {}, asRef(c, index), asRef(c, makeValue<uint64_t>(c, u64_t, nklt_sizeof(elem_t))));
            auto const offset_ref = asRef(c, makeInstr(mul, u64_t));
            auto const add = nkir_make_add({}, data_ref, offset_ref);
            auto ref = asRef(c, makeInstr(add, elem_ptr_t));
            ref.is_indirect = true;
            ref.type = tovmt(elem_t);
            return makeRef(ref);
        } else {
            auto sb = nksb_create();
            defer {
                nksb_free(sb);
            };
            nklt_inspect(type, sb);
            auto type_str = nksb_concat(sb);
            return error(c, "type `%.*s` is not indexable", type_str.size, type_str.data),
                   ValueInfo{};
        }
    } else if (node->id == n_deref) {
        DEFINE(arg, compile(c, narg0(node)));
        if (arg.type->tclass != NkType_Ptr) {
            return error(c, "pointer expected in dereference"), ValueInfo{};
        }
        auto ref = asRef(c, arg);
        ref.is_indirect = true;
        ref.type = tovmt(arg.type->as.ptr.target_type);
        return makeRef(ref);
    } else if (node->id == n_member) {
        DEFINE(lhs, compile(c, narg0(node)));
        auto name = s2nkid(narg1(node)->token->text);
        switch (nklt_tclass(lhs.type)) {
        case NkType_Fn: {
            if (isKnown(lhs)) {
                auto lhs_val = asValue(c, lhs);
                if (nklval_typeof(lhs_val)->as.fn.call_conv == NkCallConv_Nk) {
                    auto scope_it = c->fn_scopes.find(*(NkIrFunct *)nklval_data(lhs_val));
                    if (scope_it != c->fn_scopes.end()) {
                        auto &scope = *scope_it->second;
                        auto member_it = scope.locals.find(name);
                        if (member_it != scope.locals.end()) {
                            auto &decl = member_it->second;
                            return declToValueInfo(decl);
                        } else {
                            return error(c, "member `%s` not found", nkid2cs(name)), ValueInfo{};
                        }
                    }
                } else {
                    return error(
                               c,
                               "cannot subscript function with a calling convention other than "
                               "nkl"),
                           ValueInfo{};
                }
            } else {
                return error(c, "cannot subscript a runtime function"), ValueInfo{};
            }
            assert(!"unreachable");
        }

        case NklType_Struct: {
            return compileStructIndex(c, lhs, lhs.type, name);
        }

        case NklType_Slice: {
            return compileStructIndex(c, lhs, lhs.type->underlying_type, name);
        }

        default: {
            auto sb = nksb_create();
            defer {
                nksb_free(sb);
            };
            nklt_inspect(lhs.type, sb);
            auto type_str = nksb_concat(sb);
            return error(c, "type `%.*s` is not subscriptable", type_str.size, type_str.data),
                   ValueInfo{};
        }
        }
        assert(!"unreachable");
    } else {
        return error(c, "invalid lvalue"), ValueInfo{};
    }
}

template <class F>
ValueInfo compileComptimeConstDef(NklCompiler c, NklAstNode node, F const &compile_cnst) {
    auto names = nargs0(node);
    if (names.size > 1) {
        return error(c, "TODO multiple assignment is not implemented"), ValueInfo{};
    }
    auto name_str = std_str(names.data[0].token->text);
    c->comptime_const_names.push(name_str);
    defer {
        c->comptime_const_names.pop();
    };
    nkid name = s2nkid({name_str.data(), name_str.size()});
    DEFINE(cnst, compile_cnst());
    CHECK(defineComptimeConst(c, name, cnst));
    return makeVoid();
}

NkIrFunct nkl_compileFile(NklCompiler c, nkstr path);

ValueInfo compileFn(
    NklCompiler c,
    NklAstNode node,
    bool is_variadic,
    NkCallConv call_conv = NkCallConv_Nk) {
    // TODO Refactor fn compilation

    std::vector<nkid> params_names;
    std::vector<nkltype_t> params_types;

    auto params = nargs0(node);

    for (size_t i = 0; i < params.size; i++) {
        auto const param = &params.data[i];
        params_names.emplace_back(s2nkid(narg0(param)->token->text));
        DEFINE(val, comptimeCompileNodeGetValue(c, narg1(param)));
        params_types.emplace_back(nklval_as(nkltype_t, val));
    }

    nkltype_t ret_t;
    if (narg1(node)) {
        DEFINE(val, comptimeCompileNodeGetValue(c, narg1(node)));
        ret_t = nklval_as(nkltype_t, val);
    } else {
        ret_t = void_t;
    }
    nkltype_t args_t = nkl_get_tuple(c->arena, params_types.data(), params_types.size(), 1);

    auto fn_t = nkl_get_fn({ret_t, args_t, call_conv, is_variadic});

    auto fn = nkir_makeFunct(c->ir);
    auto pop_fn = pushFn(c, fn);

    std::string fn_name;
    if (c->node_stack.size() >= 2 && (*(c->node_stack.rbegin() + 1))->id == n_comptime_const_def) {
        fn_name = c->comptime_const_names.top();
    } else {
        // TODO Fix not finding the name of #extern function
        fn_name.resize(100);
        int n = std::snprintf(fn_name.data(), fn_name.size(), "fn%zu", c->fn_counter++);
        fn_name.resize(n);
    }
    nkir_startFunct(fn, {fn_name.c_str(), fn_name.size()}, tovmt(fn_t));
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), irBlockName(c, "start"));

    pushFnScope(c, fn);
    defer {
        popScope(c);
    };

    for (size_t i = 0; i < params.size; i++) {
        CHECK(defineArg(c, params_names[i], i, params_types[i]));
    }

    CHECK(compileStmt(c, narg2(node)));
    gen(c, nkir_make_ret());

    NK_LOG_INF(
        "ir:\n%s", (char const *)[&]() {
            auto sb = nksb_create();
            nkir_inspectFunct(fn, sb);
            return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                nksb_free(sb);
            });
        }());

    if (call_conv == NkCallConv_Nk) {
        return makeValue<void *>(c, fn_t, (void *)fn);
    } else if (call_conv == NkCallConv_Cdecl) {
        auto cl = nkir_makeNativeClosure(c->ir, fn);
        return makeValue<void *>(c, fn_t, *(void **)cl);
    } else {
        assert(!"invalid calling convention");
    }
}

ValueInfo compileFnType(NklCompiler c, NklAstNode node, bool is_variadic) {
    std::vector<nkid> params_names;
    std::vector<nkltype_t> params_types;

    // TODO Boilerplate between compileFn and compileFnType

    auto params = nargs0(node);

    for (size_t i = 0; i < params.size; i++) {
        auto const param = &params.data[i];
        params_names.emplace_back(s2nkid(narg0(param)->token->text));
        DEFINE(val, comptimeCompileNodeGetValue(c, narg1(param)));
        params_types.emplace_back(nklval_as(nkltype_t, val));
    }

    DEFINE(ret_t_val, comptimeCompileNodeGetValue(c, narg1(node)));
    auto ret_t = nklval_as(nkltype_t, ret_t_val);
    nkltype_t args_t = nkl_get_tuple(c->arena, params_types.data(), params_types.size(), 1);

    auto fn_t =
        nkl_get_fn({ret_t, args_t, NkCallConv_Cdecl, is_variadic}); // TODO CallConv Hack for #link

    return makeValue<nkltype_t>(c, typeref_t, fn_t);
}

void initFromAst(NklCompiler c, nklval_t val, NklAstNodeArray init_nodes) {
    switch (nklval_tclass(val)) {
    case NkType_Array:
        if (init_nodes.size > nklval_tuple_size(val)) {
            return error(c, "too many values to init array");
        }
        for (size_t i = 0; i < nklval_array_size(val); i++) {
            initFromAst(c, nklval_array_at(val, i), {&init_nodes.data[i], 1});
        }
        break;
    case NkType_Numeric: {
        if (init_nodes.size > 1) {
            return error(c, "too many values to init numeric");
        }
        auto const &node = init_nodes.data[0];
        if (node.id != n_int && node.id != n_float) {
            return error(c, "invalid value to init numeric");
        }
        auto str = std_str(node.token->text);
        switch (nklval_typeof(val)->as.num.value_type) {
        case Int8:
            nklval_as(int8_t, val) = std::stoll(str);
            break;
        case Int16:
            nklval_as(int16_t, val) = std::stoll(str);
            break;
        case Int32:
            nklval_as(int32_t, val) = std::stoll(str);
            break;
        case Int64:
            nklval_as(int64_t, val) = std::stoll(str);
            break;
        case Uint8:
            nklval_as(uint8_t, val) = std::stoull(str);
            break;
        case Uint16:
            nklval_as(uint16_t, val) = std::stoull(str);
            break;
        case Uint32:
            nklval_as(uint32_t, val) = std::stoull(str);
            break;
        case Uint64:
            nklval_as(uint64_t, val) = std::stoull(str);
            break;
        case Float32:
            nklval_as(float, val) = std::stof(str);
            break;
        case Float64:
            nklval_as(double, val) = std::stod(str);
            break;
        }
        break;
    }
    case NklType_Struct:
    case NkType_Tuple:
        if (init_nodes.size > nklval_tuple_size(val)) {
            return error(c, "too many values to init tuple/struct");
        }
        for (size_t i = 0; i < nklval_tuple_size(val); i++) {
            initFromAst(c, nklval_tuple_at(val, i), {&init_nodes.data[i], 1});
        }
        break;
    default:
        NK_LOG_ERR("initFromAst is not implemented for this type");
        assert(!"unreachable");
        break;
    }
}

ValueInfo compile(NklCompiler c, NklAstNode node) {
#ifdef BUILD_WITH_EASY_PROFILER
    auto block_name = std::string{"compile: "} + s_nkl_ast_node_names[node->id];
#endif // BUILD_WITH_EASY_PROFILER
    EASY_BLOCK(block_name.c_str(), ::profiler::colors::DeepPurple100);
    NK_LOG_DBG("node: %s", s_nkl_ast_node_names[node->id]);
    c->node_stack.emplace_back(node);
    defer {
        c->node_stack.pop_back();
    };

    switch (node->id) {
    case n_none: {
        return {};
    }

    case n_nop: {
        nkir_gen(c->ir, nkir_make_nop()); // TODO Do we actually need to generate nop?
        return makeVoid();
    }

    case n_false: {
        return makeValue<bool>(c, bool_t, false);
    }

    case n_true: {
        return makeValue<bool>(c, bool_t, true);
    }

#define X(NAME, VALUE_TYPE, CTYPE)                                \
    case CAT(n_, NAME): {                                         \
        return makeValue<nkltype_t>(c, typeref_t, CAT(NAME, _t)); \
    }
        NUMERIC_ITERATE(X)
#undef X

    case n_bool: {
        return makeValue<nkltype_t>(c, typeref_t, bool_t);
    }

    case n_type_t: {
        return makeValue<nkltype_t>(c, typeref_t, typeref_t);
    }

    case n_void: {
        return makeValue<nkltype_t>(c, typeref_t, void_t);
    }

    case n_compl: {
        DEFINE(arg, compile(c, narg0(node)));
        if (arg.type->tclass != NkType_Numeric) {
            return error(c, "expected number in two's complement"), ValueInfo{};
        }
        return makeInstr(nkir_make_compl({}, asRef(c, arg)), arg.type);
    }

    case n_not: {
        DEFINE(arg, compile(c, narg0(node)));
        // TODO Modeling bool as u8
        if (arg.type->tclass != NkType_Numeric || arg.type->as.num.value_type != Uint8) {
            return error(c, "expected bool in negation"), ValueInfo{};
        }
        return makeInstr(nkir_make_not({}, asRef(c, arg)), arg.type);
    }

    case n_uminus: {
        DEFINE(arg, compile(c, narg0(node)));
        if (arg.type->tclass != NkType_Numeric) {
            return error(c, "expected number in unary minus"), ValueInfo{};
        }
        return makeInstr(nkir_make_neg({}, asRef(c, arg)), arg.type);
    }

    case n_uplus: {
        DEFINE(arg, compile(c, narg0(node)));
        if (arg.type->tclass != NkType_Numeric) {
            return error(c, "expected number in unary plus"), ValueInfo{};
        }
        return compile(c, narg0(node));
    }

    case n_addr: {
        DEFINE(arg, compile(c, narg0(node)));
        return makeInstr(nkir_make_lea({}, asRef(c, arg)), nkl_get_ptr(arg.type));
    }

    case n_deref: {
        return getLvalueRef(c, node);
    }

    case n_return: {
        if (nargs0(node).size) {
            DEFINE(arg, compile(c, narg0(node))); // TODO ignoring multiple return values
            CHECK(store(c, nkir_makeRetRef(c->ir), arg));
        }
        gen(c, nkir_make_ret());
        return makeVoid();
    }

    case n_ptr_type: {
        DEFINE(val, comptimeCompileNodeGetValue(c, narg0(node)));
        auto target_type = nklval_as(nkltype_t, val);
        return makeValue<nkltype_t>(c, typeref_t, nkl_get_ptr(target_type));
    }

    case n_const_ptr_type: {
        // TODO Ignoring const in const_ptr_type
        DEFINE(val, comptimeCompileNodeGetValue(c, narg0(node)));
        auto target_type = nklval_as(nkltype_t, val);
        return makeValue<nkltype_t>(c, typeref_t, nkl_get_ptr(target_type));
    }

    case n_slice_type: {
        DEFINE(val, comptimeCompileNodeGetValue(c, narg0(node)));
        auto target_type = nklval_as(nkltype_t, val);
        auto type = nkl_get_slice(c->arena, target_type);
        return makeValue<nkltype_t>(c, typeref_t, type);
    }

    case n_scope: {
        // TODO scope compilation is disabled
        // gen(c, nkir_make_enter());
        // defer {
        //     gen(c, nkir_make_leave());
        // };
        return compile(c, narg0(node));
    }

    case n_run: {
        DEFINE(arg, comptimeCompileNodeGetValue(c, narg0(node)));
        return makeValue(c, arg);
    }

    // TODO Not doing type checks in arithmetic

#define COMPILE_BIN(NAME)                                                                    \
    case CAT(n_, NAME): {                                                                    \
        DEFINE(lhs, compile(c, narg0(node)));                                                \
        DEFINE(rhs, compile(c, narg1(node)));                                                \
        return makeInstr(CAT(nkir_make_, NAME)({}, asRef(c, lhs), asRef(c, rhs)), lhs.type); \
    }

        COMPILE_BIN(add)
        COMPILE_BIN(sub)
        COMPILE_BIN(mul)
        COMPILE_BIN(div)
        COMPILE_BIN(mod)

        COMPILE_BIN(bitand)
        COMPILE_BIN(bitor)
        COMPILE_BIN(xor)
        COMPILE_BIN(lsh)
        COMPILE_BIN(rsh)

#undef COMPILE_BIN

#define COMPILE_BOOL_BIN(NAME)                                                             \
    case CAT(n_, NAME): {                                                                  \
        DEFINE(lhs, compile(c, narg0(node)));                                              \
        DEFINE(rhs, compile(c, narg1(node)));                                              \
        return makeInstr(CAT(nkir_make_, NAME)({}, asRef(c, lhs), asRef(c, rhs)), bool_t); \
    }

        COMPILE_BOOL_BIN(and)
        COMPILE_BOOL_BIN(or)

        COMPILE_BOOL_BIN(eq)
        COMPILE_BOOL_BIN(ge)
        COMPILE_BOOL_BIN(gt)
        COMPILE_BOOL_BIN(le)
        COMPILE_BOOL_BIN(lt)
        COMPILE_BOOL_BIN(ne)

#undef COMPILE_BOOL_BIN

    case n_array_type: {
        // TODO Hardcoded array size type in array_type
        DEFINE(type_val, comptimeCompileNodeGetValue(c, narg0(node)));
        auto type = nklval_as(nkltype_t, type_val);
        DEFINE(size_val, comptimeCompileNodeGetValue(c, narg1(node)));
        auto size = nklval_as(int64_t, size_val);
        return makeValue<nkltype_t>(c, typeref_t, nkl_get_array(type, size));
    }

    case n_cast: {
        DEFINE(const dst_type_val, comptimeCompileNodeGetValue(c, narg0(node)));
        if (nklval_tclass(dst_type_val) != NklType_Typeref) {
            return error(c, "type expected in cast"), ValueInfo{};
        }
        auto const dst_type = nklval_as(nkltype_t, dst_type_val);

        DEFINE(src, compile(c, narg1(node)));
        auto const src_type = src.type;

        switch (dst_type->tclass) {
        case NkType_Numeric: {
            switch (src_type->tclass) {
            case NkType_Numeric: {
                return makeInstr(nkir_make_cast({}, tovmt(dst_type), asRef(c, src)), dst_type);
            }

            case NkType_Ptr: {
                if (dst_type->as.num.value_type != Uint64) {
                    return error(c, "cannot cast pointer to any numeric type other than u64"),
                           ValueInfo{};
                }
                src.type = dst_type;
                return src;
            }
            }
            break;
        }

        case NkType_Ptr: {
            switch (src_type->tclass) {
            case NkType_Numeric: {
                if (src_type->as.num.value_type != Uint64) {
                    return error(c, "cannot cast any numeric type other than u64 to a pointer"),
                           ValueInfo{};
                }
                src.type = dst_type;
                return src;
            }

            case NkType_Ptr: {
                src.type = dst_type;
                return src;
            }
            }
            break;
        }

        case NkType_Fn: {
            switch (src_type->tclass) {
            case NkType_Fn: {
                src.type = dst_type;
                return src;
            }
            }
            break;
        }

        case NkType_Void: {
            (void)asRef(c, src);
            return makeVoid();
        }
        }

        // TODO Clumsy error printing again
        auto dst_sb = nksb_create();
        auto src_sb = nksb_create();
        defer {
            nksb_free(dst_sb);
            nksb_free(src_sb);
        };
        nklt_inspect(dst_type, dst_sb);
        nklt_inspect(src_type, src_sb);
        auto dst_type_str = nksb_concat(dst_sb);
        auto src_type_str = nksb_concat(src_sb);
        return error(
                   c,
                   "cannot cast value of type `%.*s` to type `%.*s`",
                   src_type_str.size,
                   src_type_str.data,
                   dst_type_str.size,
                   dst_type_str.data),
               ValueInfo{};
    }

    case n_index: {
        return getLvalueRef(c, node);
    }

    case n_while: {
        auto l_loop = nkir_makeBlock(c->ir);
        auto l_endloop = nkir_makeBlock(c->ir);
        nkir_startBlock(c->ir, l_loop, irBlockName(c, "loop"));
        gen(c, nkir_make_enter());
        DEFINE(cond, compile(c, narg0(node)));
        gen(c, nkir_make_jmpz(asRef(c, cond), l_endloop));
        CHECK(compileStmt(c, narg1(node)));
        gen(c, nkir_make_leave());
        gen(c, nkir_make_jmp(l_loop));
        nkir_startBlock(c->ir, l_endloop, irBlockName(c, "endloop"));
        return makeVoid();
    }

    case n_if: {
        auto l_endif = nkir_makeBlock(c->ir);
        NkIrBlockId l_else;
        if (narg2(node) && narg2(node)->id) {
            l_else = nkir_makeBlock(c->ir);
        } else {
            l_else = l_endif;
        }
        DEFINE(cond, compile(c, narg0(node)));
        gen(c, nkir_make_jmpz(asRef(c, cond), l_else));
        {
            pushScope(c);
            defer {
                popScope(c);
            };
            CHECK(compileStmt(c, narg1(node)));
        }
        if (narg2(node) && narg2(node)->id) {
            gen(c, nkir_make_jmp(l_endif));
            nkir_startBlock(c->ir, l_else, irBlockName(c, "else"));
            {
                pushScope(c);
                defer {
                    popScope(c);
                };
                CHECK(compileStmt(c, narg2(node)));
            }
        }
        nkir_startBlock(c->ir, l_endif, irBlockName(c, "endif"));
        return makeVoid();
    }

    case n_block: {
        auto nodes = nargs0(node);
        for (size_t i = 0; i < nodes.size; i++) {
            CHECK(compileStmt(c, &nodes.data[i]));
        }
        return makeVoid();
    }

    case n_tuple_type: {
        auto nodes = nargs0(node);
        std::vector<nkltype_t> types;
        types.reserve(nodes.size);
        for (size_t i = 0; i < nodes.size; i++) {
            DEFINE(val, comptimeCompileNodeGetValue(c, &nodes.data[i]));
            if (nklval_tclass(val) != NklType_Typeref) {
                return error(c, "type expected in tuple type"), ValueInfo{};
            }
            types.emplace_back(nklval_as(nkltype_t, val));
        }
        return makeValue<nkltype_t>(
            c, typeref_t, nkl_get_tuple(c->arena, types.data(), types.size(), 1));
    }

    case n_import: {
        NK_LOG_WRN("TODO import implementation not finished");
        auto name = narg0(node)->token->text;
        std::string filename = std_str(name) + ".nkl";
        auto stdlib_path = fs::path{c->stdlib_dir};
        if (!stdlib_path.is_absolute()) {
            stdlib_path = fs::path{c->compiler_dir} / stdlib_path;
        }
        auto filepath = stdlib_path / filename;
        auto filepath_str = filepath.string();
        if (!fs::exists(filepath)) {
            return error(
                       c,
                       "imported file `%.*s` doesn't exist",
                       filepath_str.size(),
                       filepath_str.c_str()),
                   ValueInfo{};
        }
        auto it = c->imports.find(filepath);
        if (it == c->imports.end()) {
            DEFINE(fn, nkl_compileFile(c, {filepath_str.c_str(), filepath_str.size()}));
            std::tie(it, std::ignore) = c->imports.emplace(filepath, fn);
        }
        auto fn = it->second;
        auto fn_val = asValue(c, makeValue<decltype(fn)>(c, fromvmt(nkir_functGetType(fn)), fn));
        CHECK(defineComptimeConst(c, s2nkid(name), makeValueComptimeConst(fn_val)));
        return makeVoid();
    }

    case n_id: {
        nkstr name_str = node->token->text;
        nkid name = s2nkid(name_str);
        auto &decl = resolve(c, name);
        if (decl.kind == Decl_Undefined) {
            return error(c, "`%.*s` is not defined", name_str.size, name_str.data), ValueInfo{};
        } else {
            return declToValueInfo(decl);
        }
    }

    case n_float: {
        double value = 0.0;
        // TODO Replace sscanf in Compiler
        int res = std::sscanf(node->token->text.data, "%lf", &value);
        (void)res;
        assert(res > 0 && res != EOF && "integer constant parsing failed");
        return makeValue<double>(c, f64_t, value);
    }

    case n_int: {
        int64_t value = 0;
        // TODO Replace sscanf in Compiler
        int res = std::sscanf(node->token->text.data, "%ld", &value);
        (void)res;
        assert(res > 0 && res != EOF && "integer constant parsing failed");
        return makeValue<int64_t>(c, i64_t, value);
    }

    case n_string: {
        auto size = node->token->text.size;

        auto ar_t = nkl_get_array(i8_t, size + 1);
        auto str_t = nkl_get_ptr(ar_t);

        auto str = (char *)nk_allocate(c->arena, size + 1);
        std::memcpy(str, node->token->text.data, size);
        str[size] = '\0';

        return makeValue<void *>(c, str_t, (void *)str);
    }

    case n_escaped_string: {
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };
        nksb_str_unescape(sb, node->token->text);
        auto unescaped_str = nksb_concat(sb);

        auto size = unescaped_str.size;

        auto ar_t = nkl_get_array(i8_t, size + 1);
        auto str_t = nkl_get_ptr(ar_t);

        auto str = (char *)nk_allocate(c->arena, unescaped_str.size + 1);
        std::memcpy(str, unescaped_str.data, size);
        str[size] = '\0';

        return makeValue<void *>(c, str_t, (void *)str);
    }

    case n_member: {
        return getLvalueRef(c, node);
    }

    case n_struct: {
        std::vector<NklStructField> fields{};

        auto nodes = nargs0(node);
        for (size_t i = 0; i < nodes.size; i++) {
            nkid name = s2nkid(narg0(&nodes.data[i])->token->text);
            DEFINE(type_val, comptimeCompileNodeGetValue(c, narg1(&nodes.data[i])));
            if (nklval_tclass(type_val) != NklType_Typeref) {
                return error(c, "type expected in tuple type"), ValueInfo{};
            }
            fields.emplace_back(NklStructField{
                .name = name,
                .type = nklval_as(nkltype_t, type_val),
            });
        }

        auto struct_type = nkl_get_struct(c->arena, fields.data(), fields.size());
        return makeValue<nkltype_t>(c, typeref_t, struct_type);
    }

    case n_fn: {
        return compileFn(c, node, false);
    }

    case n_fn_type: {
        return compileFnType(c, node, false);
    }

    case n_fn_var: {
        return compileFn(c, node, true);
    }

    case n_fn_type_var: {
        return compileFnType(c, node, true);
    }

    case n_tag: {
        NK_LOG_WRN("TODO Only handling #link and #extern tags");

        auto n_tag_name = narg0(node);
        auto n_args = nargs1(node);
        auto n_def = narg2(node);

        std::string tag_name{n_tag_name->token->text.data, n_tag_name->token->text.size};

        if (n_def->id != n_comptime_const_def) {
            return error(c, "TODO comptime const def assumed in tag"), ValueInfo{};
        }

        if (tag_name == "#link") {
            assert(n_args.size == 1 || n_args.size == 2);
            DEFINE(soname_val, comptimeCompileNodeGetValue(c, narg1(&n_args.data[0])));
            std::string soname = nklval_as(char const *, soname_val);
            if (soname == "c" || soname == "C") {
                soname = c->libc_name;
            } else if (soname == "m" || soname == "M") {
                soname = c->libm_name;
            }
            std::string link_prefix;
            if (n_args.size == 2) {
                DEFINE(link_prefix_val, comptimeCompileNodeGetValue(c, narg1(&n_args.data[1])));
                link_prefix = nklval_as(char const *, link_prefix_val);
            }

            auto so = nkir_makeShObj(c->ir, cs2s(soname.c_str())); // TODO Creating so every time

            auto sym_name = narg0(n_def)->token->text;
            auto sym_name_with_prefix_std_str = link_prefix + std_str(sym_name);
            nkstr sym_name_with_prefix{
                sym_name_with_prefix_std_str.data(), sym_name_with_prefix_std_str.size()};

            DEFINE(fn_t_val, comptimeCompileNodeGetValue(c, narg1(n_def)));
            auto fn_t = nklval_as(nkltype_t, fn_t_val);

            CHECK(defineExtSym(
                c,
                s2nkid(sym_name),
                nkir_makeExtSym(c->ir, so, sym_name_with_prefix, tovmt(fn_t)),
                fn_t));
        } else if (tag_name == "#extern") {
            CHECK(compileComptimeConstDef(c, n_def, [=]() -> ComptimeConst {
                auto const n_def_id = narg1(n_def)->id;
                if (n_def_id != n_fn && n_def_id != n_fn_var) {
                    return error(c, "function expected in #extern"), ComptimeConst{};
                }
                bool const is_variadic = n_def_id == n_fn_var;
                DEFINE(fn_info, compileFn(c, narg1(n_def), is_variadic, NkCallConv_Cdecl));
                DEFINE(fn_val, asValue(c, fn_info));
                return makeValueComptimeConst(fn_val);
            }));
        } else {
            assert(!"unsupported tag");
        }
        return makeVoid();
    }

    case n_call: {
        DEFINE(lhs, compile(c, narg0(node)));

        if (lhs.type->tclass != NkType_Fn) {
            return error(c, "function expected in call"), ValueInfo{};
        }

        auto const fn_t = lhs.type;

        std::vector<ValueInfo> args_info{};
        std::vector<nkltype_t> args_types{};

        auto nodes = nargs1(node);
        for (size_t i = 0; i < nodes.size; i++) {
            // TODO Support named args in call
            DEFINE(val_info, compile(c, narg1(&nodes.data[i])));
            args_info.emplace_back(val_info);
            args_types.emplace_back(
                i < fn_t->as.fn.args_t->as.tuple.elems.size
                    ? fn_t->as.fn.args_t->as.tuple.elems.data[i].type
                    : val_info.type);
        }

        auto args_t = nkl_get_tuple(c->arena, args_types.data(), args_types.size(), 1);

        NkIrRef args{};
        if (nklt_sizeof(args_t)) {
            args = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(args_t)));
        }

        for (size_t i = 0; i < args_info.size(); i++) {
            auto arg_ref = args;
            arg_ref.offset += args_t->as.tuple.elems.data[i].offset;
            arg_ref.type = tovmt(args_t->as.tuple.elems.data[i].type);
            CHECK(store(c, arg_ref, args_info[i]));
        }

        return makeInstr(nkir_make_call({}, asRef(c, lhs), args), fn_t->as.fn.ret_t);
    }

    case n_object_literal: {
        // TODO Optimize object literal

        DEFINE(type_val, comptimeCompileNodeGetValue(c, narg0(node)));

        if (nklval_tclass(type_val) != NklType_Typeref) {
            return error(c, "type expected in object literal"), ValueInfo{};
        }

        auto type = nklval_as(nkltype_t, type_val);

        nklval_t val{nk_allocate(c->arena, nklt_sizeof(type)), type};
        std::memset(nklval_data(val), 0, nklval_sizeof(val));

        auto init_nodes = nargs1(node);

        // TODO Ignoring named args in object literal, and copying values to a sperate array
        std::vector<NklAstNode_T> nodes;
        nodes.reserve(init_nodes.size);

        for (size_t i = 0; i < init_nodes.size; i++) {
            nodes.emplace_back(narg1(&init_nodes.data[i])[0]);
        }

        CHECK(initFromAst(c, val, {nodes.data(), nodes.size()}));

        return makeValue(c, val);
    }

    case n_assign: {
        if (nargs0(node).size > 1) {
            NK_LOG_WRN("TODO multiple assignment is not supported");
        }
        DEFINE(lhs, getLvalueRef(c, narg0(node)));
        DEFINE(rhs, compile(c, narg1(node)));
        DEFINE(ref, store(c, asRef(c, lhs), rhs));
        return ref;
    }

    case n_define: {
        // TODO Shadowing is not allowed
        auto const &names = nargs0(node);
        if (names.size > 1) {
            return error(c, "TODO multiple assignment is not implemented"), ValueInfo{};
        }
        nkid name = s2nkid(names.data[0].token->text);
        DEFINE(rhs, compile(c, narg1(node)));
        NkIrRef ref;
        if (curScope(c).is_top_level) {
            auto var = nkir_makeGlobalVar(c->ir, tovmt(rhs.type));
            CHECK(defineGlobal(c, name, var, rhs.type));
            ref = nkir_makeGlobalRef(c->ir, var);
        } else {
            auto var = nkir_makeLocalVar(c->ir, tovmt(rhs.type));
            CHECK(defineLocal(c, name, var, rhs.type));
            ref = nkir_makeFrameRef(c->ir, var);
        }
        CHECK(store(c, ref, rhs));
        return makeVoid();
    }

    case n_comptime_const_def: {
        return compileComptimeConstDef(c, node, [=]() {
            return comptimeCompileNode(c, narg1(node));
        });
    }

    case n_var_decl: {
        auto const &names = nargs0(node);
        if (names.size > 1) {
            return error(c, "TODO multiple assignment is not implemented"), ValueInfo{};
        }
        nkid name = s2nkid(names.data[0].token->text);
        DEFINE(type_val, comptimeCompileNodeGetValue(c, narg1(node)));
        auto type = nklval_as(nkltype_t, type_val);
        NkIrRef ref;
        if (curScope(c).is_top_level) {
            auto var = nkir_makeGlobalVar(c->ir, tovmt(type));
            CHECK(defineGlobal(c, name, var, type));
            ref = nkir_makeGlobalRef(c->ir, var);
        } else {
            auto var = nkir_makeLocalVar(c->ir, tovmt(type));
            CHECK(defineLocal(c, name, var, type));
            ref = nkir_makeFrameRef(c->ir, var);
        }
        if (narg2(node) && narg2(node)->id) {
            DEFINE(val, compile(c, narg2(node)));
            CHECK(store(c, ref, val));
        }
        return makeVoid();
    }

    default:
        return error(c, "unknown ast node"), ValueInfo{};
    }
}

ComptimeConst comptimeCompileNode(NklCompiler c, NklAstNode node) {
    auto fn = nkir_makeFunct(c->ir);
    NkltFnInfo fn_info{nullptr, nkl_get_tuple(c->arena, nullptr, 0, 1), NkCallConv_Nk, false};

    auto pop_fn = pushFn(c, fn);

    auto const vm_fn_info = tovmf(fn_info);
    nkir_startIncompleteFunct(fn, cs2s("#comptime"), &vm_fn_info);
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), irBlockName(c, "start"));

    pushFnScope(c, fn);
    defer {
        popScope(c);
    };

    DEFINE(cnst_val, compile(c, node));
    nkir_incompleteFunctGetInfo(fn)->ret_t = tovmt(cnst_val.type);
    nkir_finalizeIncompleteFunct(fn, tovmt(nkl_get_fn(fromvmf(*nkir_incompleteFunctGetInfo(fn)))));

    ComptimeConst cnst{};

    if (isKnown(cnst_val)) {
        nkir_discardFunct(fn);
        c->fn_scopes.erase(fn); // TODO Actually delete persistent scopes

        cnst = makeValueComptimeConst(asValue(c, cnst_val));
    } else {
        CHECK(store(c, nkir_makeRetRef(c->ir), cnst_val));
        gen(c, nkir_make_ret());

        cnst = makeFunctComptimeConst(fn);

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

nklval_t comptimeCompileNodeGetValue(NklCompiler c, NklAstNode node) {
    DEFINE(cnst, comptimeCompileNode(c, node));
    // TODO Probably should have a way of controlling, whether we want to
    // automatically invoke code at compile time here
    return comptimeConstGetValue(c, cnst);
}

Void compileStmt(NklCompiler c, NklAstNode node) {
    DEFINE(val, compile(c, node));
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
    return {};
}

NkIrFunct nkl_compile(NklCompiler c, NklAstNode root) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);

    auto fn = nkir_makeFunct(c->ir);
    auto pop_fn = pushFn(c, fn);

    auto top_level_fn_t =
        nkl_get_fn({void_t, nkl_get_tuple(c->arena, nullptr, 0, 1), NkCallConv_Nk, false});

    nkir_startFunct(fn, cs2s("#top_level"), tovmt(top_level_fn_t));
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), irBlockName(c, "start"));

    pushTopLevelFnScope(c, fn);
    defer {
        popScope(c);
    };

    if (root) {
        c->cur_fn = fn;
        CHECK(compileStmt(c, root));
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

void printQuote(std::string_view src, NklTokenRef token, bool to_color) {
    auto prev_newline = src.find_last_of("\n", token->pos);
    if (prev_newline == std::string::npos) {
        prev_newline = 0;
    } else {
        prev_newline++;
    }
    auto next_newline = src.find_first_of("\n", token->pos);
    std::fprintf(
        stderr,
        "%zu | %.*s%s%.*s%s%.*s\n%s%*s",
        token->lin,
        (int)(token->pos - prev_newline),
        src.data() + prev_newline,
        to_color ? TERM_COLOR_RED : "",
        (int)(token->text.size),
        src.data() + token->pos,
        to_color ? TERM_COLOR_NONE : "",
        (int)(next_newline - token->pos - token->text.size),
        src.data() + token->pos + token->text.size,
        to_color ? TERM_COLOR_RED : "",
        (int)(token->col + std::log10(token->lin) + 4),
        "^");
    if (token->text.size) {
        for (size_t i = 0; i < token->text.size - 1; i++) {
            std::fprintf(stderr, "%c", '~');
        }
    }
    std::fprintf(stderr, "%s\n", to_color ? TERM_COLOR_NONE : "");
}

void printError(NklCompiler c, NklTokenRef token, std::string const &err_str) {
    // TODO Refactor coloring
    // TODO Add option to control coloring from CLI
    bool const to_color = nksys_isatty();
    assert(!c->file_stack.empty());
    std::fprintf(
        stderr,
        "%s%s:%zu:%zu:%s %serror:%s %.*s\n",
        to_color ? TERM_COLOR_WHITE : "",
        c->file_stack.top().string().c_str(),
        token->lin,
        token->col,
        to_color ? TERM_COLOR_NONE : "",
        to_color ? TERM_COLOR_RED : "",
        to_color ? TERM_COLOR_NONE : "",
        (int)err_str.size(),
        err_str.data());

    assert(!c->src_stack.empty());
    auto src = std_view(c->src_stack.top());

    printQuote(src, token, to_color);

    c->error_occurred = true;
}

void printError(NklCompiler c, char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    auto str = string_vformat(fmt, ap);
    va_end(ap);

    bool const to_color = nksys_isatty();

    std::fprintf(
        stderr,
        "%serror:%s %.*s\n",
        to_color ? TERM_COLOR_RED : "",
        to_color ? TERM_COLOR_NONE : "",
        (int)str.size(),
        str.c_str());

    c->error_occurred = true;
}

NkIrFunct nkl_compileSrc(NklCompiler c, nkstr src) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);

    c->src_stack.emplace(src);
    defer {
        c->src_stack.pop();
    };

    std::string &err_str = c->err_str;
    NklTokenRef &err_token = c->err_token;

    std::vector<NklToken> tokens;
    bool res = nkl_lex(src, tokens, err_str);
    if (!res) {
        printError(c, &tokens.back(), err_str);
        return {};
    }

    auto ast = nkl_ast_create();
    defer {
        nkl_ast_free(ast);
    };
    auto root = nkl_parse(ast, tokens, err_str, err_token);
    if (!root) {
        printError(c, err_token, err_str);
        return {};
    }

    auto fn = nkl_compile(c, root);
    if (c->error_occurred) {
        printError(c, c->err_token, err_str);
        return {};
    }

    return fn;
}

NkIrFunct nkl_compileFile(NklCompiler c, nkstr path) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);

    c->file_stack.emplace(fs::relative(fs::path{std_str(path)}));
    defer {
        c->file_stack.pop();
    };

    std::ifstream file{std_str(path)};
    if (file) {
        std::string src{std::istreambuf_iterator<char>{file}, {}};
        return nkl_compileSrc(c, {src.data(), src.size()});
    } else {
        printError(c, "failed to open file `%.*s`", (int)path.size, path.data);
        return {};
    }
}

template <class T>
T getConfigValue(NklCompiler c, std::string const &name, decltype(Scope::locals) &config) {
    auto it = config.find(cs2nkid(name.c_str()));
    ValueInfo val_info;
    if (it == config.end()) {
        error(c, "`%.*s` is missing in config", name.size(), name.c_str());
    } else {
        val_info = declToValueInfo(it->second);
        if (!isKnown(val_info)) {
            error(c, "`%.*s` value is not known", name.size(), name.c_str());
        }
    }
    if (c->error_occurred) {
        printError(c, "%.*s", (int)c->err_str.size(), c->err_str.c_str());
        return T{};
    }
    return nklval_as(T, asValue(c, val_info)); // TODO Reinterpret cast in compiler without check
}

} // namespace

extern "C" NK_EXPORT Void nkl_compiler_declareLocal(nkstr name, nkltype_t type) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);
    NklCompiler c = s_compiler;
    // TODO Treating slice as cstring, while we include excess zero charater
    CHECK(defineLocal(
        c, cs2nkid(std_str(name).c_str()), nkir_makeLocalVar(c->ir, tovmt(type)), type));
    return {};
}

struct NklCompilerBuilder {
    std::vector<fs::path> libs{};
};

extern "C" NK_EXPORT NklCompilerBuilder *nkl_compiler_createBuilder() {
    NK_LOG_TRC(__func__);

    return new (nk_allocate(nk_default_allocator, sizeof(NklCompilerBuilder))) NklCompilerBuilder{};
}

extern "C" NK_EXPORT void nkl_compiler_freeBuilder(NklCompilerBuilder *b) {
    NK_LOG_TRC(__func__);

    b->~NklCompilerBuilder();
    nk_free(nk_default_allocator, b);
}

extern "C" NK_EXPORT void nkl_compiler_link(NklCompilerBuilder *b, nkstr lib) {
    NK_LOG_TRC(__func__);

    b->libs.emplace_back(std_view(lib));
}

extern "C" NK_EXPORT bool nkl_compiler_build(
    NklCompilerBuilder *b,
    NkIrFunct entry,
    nkstr exe_name) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);

    NklCompiler c = s_compiler;

    std::string flags = c->c_compiler_flags;
    for (auto const &lib : b->libs) {
        if (!lib.parent_path().empty()) {
            flags += " -L" + lib.parent_path().string();
        }
        flags += " -l:" + lib.filename().string();
    }

    NkIrCompilerConfig conf{
        .compiler_binary = {c->c_compiler.c_str(), c->c_compiler.size()},
        .additional_flags = {flags.c_str(), flags.size()},
        .output_filename = exe_name,
        .echo_src = 0,
        .quiet = 0,
    };
    return nkir_compile(conf, c->ir, entry);
}

struct StructField {
    nkstr name;
    nkltype_t type;
};

extern "C" NK_EXPORT nkltype_t nkl_compiler_makeStruct(nkslice<StructField> fields_raw) {
    NklCompiler c = s_compiler;
    std::vector<NklStructField> fields;
    fields.reserve(fields_raw.size());
    for (auto const &field : fields_raw) {
        fields.emplace_back(NklStructField{
            .name = s2nkid(field.name),
            .type = field.type,
        });
    }
    return nkl_get_struct(c->arena, fields.data(), fields.size());
}

NklCompiler nkl_compiler_create() {
    nkl_ast_init();

    u8_t = nkl_get_numeric(Uint8);
    u16_t = nkl_get_numeric(Uint16);
    u32_t = nkl_get_numeric(Uint32);
    u64_t = nkl_get_numeric(Uint64);
    i8_t = nkl_get_numeric(Int8);
    i16_t = nkl_get_numeric(Int16);
    i32_t = nkl_get_numeric(Int32);
    i64_t = nkl_get_numeric(Int64);
    f32_t = nkl_get_numeric(Float32);
    f64_t = nkl_get_numeric(Float64);

    typeref_t = nkl_get_typeref();
    void_t = nkl_get_void();
    bool_t = u8_t; // Modeling bool as u8
    u8_ptr_t = nkl_get_ptr(u8_t);

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

bool nkl_compiler_configure(NklCompiler c, nkstr config_dir) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);
    NK_LOG_DBG("config_dir=`%.*s`", config_dir.size, config_dir.data);
    c->compiler_dir = std_str(config_dir);
    auto config_filepath = fs::path{c->compiler_dir} / "config.nkl";
    auto filepath_str = config_filepath.string();
    if (!fs::exists(config_filepath)) {
        printError(
            c, "config file `%.*s` doesn't exist", (int)filepath_str.size(), filepath_str.c_str());
        return false;
    }
    auto fn = nkl_compileFile(c, {filepath_str.c_str(), filepath_str.size()});
    if (!fn) {
        return false;
    }
    auto &config = c->fn_scopes[fn]->locals;

    DEFINE(stdlib_dir_str, getConfigValue<char const *>(c, "stdlib_dir", config));
    c->stdlib_dir = stdlib_dir_str;
    NK_LOG_DBG("stdlib_dir=`%.*s`", c->stdlib_dir.size(), c->stdlib_dir.c_str());

    DEFINE(libc_name_str, getConfigValue<char const *>(c, "libc_name", config));
    c->libc_name = libc_name_str;
    NK_LOG_DBG("libc_name=`%.*s`", c->libc_name.size(), c->libc_name.c_str());

    DEFINE(libm_name_str, getConfigValue<char const *>(c, "libm_name", config));
    c->libm_name = libm_name_str;
    NK_LOG_DBG("libm_name=`%.*s`", c->libm_name.size(), c->libm_name.c_str());

    DEFINE(c_compiler_str, getConfigValue<char const *>(c, "c_compiler", config));
    c->c_compiler = c_compiler_str;
    NK_LOG_DBG("c_compiler=`%.*s`", c->c_compiler.size(), c->c_compiler.c_str());

    DEFINE(c_compiler_flags_str, getConfigValue<char const *>(c, "c_compiler_flags", config));
    c->c_compiler_flags = c_compiler_flags_str;
    NK_LOG_DBG("c_compiler_flags=`%.*s`", c->c_compiler_flags.size(), c->c_compiler_flags.c_str());

    return true;
}

// TODO nkl_compiler_run probably is not needed
bool nkl_compiler_run(NklCompiler c, NklAstNode root) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);

    // TODO Boilerplate in nkl_compiler_run_*

    auto prev_compiler = s_compiler;
    s_compiler = c;
    defer {
        s_compiler = prev_compiler;
    };

    DEFINE(fn, nkl_compile(c, root));
    nkir_invoke({&fn, nkir_functGetType(fn)}, {}, {});
    return true;
}

bool nkl_compiler_runSrc(NklCompiler c, nkstr src) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);

    auto prev_compiler = s_compiler;
    s_compiler = c;
    defer {
        s_compiler = prev_compiler;
    };

    c->file_stack.emplace("<anonymous>");
    defer {
        c->file_stack.pop();
    };

    DEFINE(fn, nkl_compileSrc(c, src));
    nkir_invoke({&fn, nkir_functGetType(fn)}, {}, {});
    return true;
}

bool nkl_compiler_runFile(NklCompiler c, nkstr path) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);

    auto prev_compiler = s_compiler;
    s_compiler = c;
    defer {
        s_compiler = prev_compiler;
    };

    DEFINE(fn, nkl_compileFile(c, path));
    nkir_invoke({&fn, nkir_functGetType(fn)}, {}, {});
    return true;
}
