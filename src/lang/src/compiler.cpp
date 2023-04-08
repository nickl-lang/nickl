#include "nkl/lang/compiler.h"

#include <algorithm>
#include <cassert>
#include <cinttypes>
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

#ifndef SCNi8
#define SCNi8 "hhi"
#endif // SCNi8

#ifndef SCNu8
#define SCNu8 "hhu"
#endif // SCNu8

#define SCNf32 "f"
#define SCNf64 "lf"

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

nkltype_t any_t;
nkltype_t typeref_t;
nkltype_t void_t;
nkltype_t void_ptr_t;
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

struct NodeInfo {
    NklAstNode node;
    nkltype_t type;
};

struct NklCompiler_T {
    NkIrProg ir;
    NkAllocator arena;

    std::string err_str{};
    NklTokenRef err_token{};
    bool error_occurred = false;
    bool error_reported = false;

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

    std::vector<NodeInfo> node_stack{};
    std::stack<std::string> comptime_const_names{};
    size_t fn_counter{};

    std::unordered_map<nkid, size_t> id2blocknum{};

    std::stack<fs::path> file_stack{};
    std::stack<nkstr> src_stack{};

    std::unordered_map<nkl_typeid_t, std::unordered_map<nkid, ValueInfo>> struct_inits{};
};

namespace {

void error(NklCompiler c, char const *fmt, ...) {
    assert(!c->error_occurred && "compiler error already initialized");

    va_list ap;
    va_start(ap, fmt);
    c->err_str = string_vformat(fmt, ap);
    va_end(ap);

    if (!c->node_stack.empty()) {
        c->err_token = c->node_stack.back().node->token;
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
    NkIrRef ref{};

    switch (val.kind) {
    case v_val: {
        ref = nkir_makeConstRef(c->ir, val.as.cnst);
        break;
    }

    case v_ref:
        ref = val.as.ref;
        break;

    case v_instr: {
        auto instr = val.as.instr;
        auto &dst = instr.arg[0].ref;
        if (dst.ref_type == NkIrRef_None && val.type->tclass != NkType_Void) {
            dst = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(val.type)));
        }
        gen(c, instr);
        ref = dst;
        break;
    }

    case v_decl: {
        auto &decl = *val.as.decl;
        switch (decl.kind) {
        case Decl_ComptimeConst:
            ref = nkir_makeConstRef(
                c->ir,
                nkir_makeConst(c->ir, tovmv(comptimeConstGetValue(c, decl.as.comptime_const))));
            break;
        case Decl_Local:
            ref = nkir_makeFrameRef(c->ir, decl.as.local.id);
            break;
        case Decl_Global:
            ref = nkir_makeGlobalRef(c->ir, decl.as.global.id);
            break;
        case Decl_ExtSym:
            ref = nkir_makeExtSymRef(c->ir, decl.as.ext_sym.id);
            break;
        case Decl_Arg:
            ref = nkir_makeArgRef(c->ir, decl.as.arg.index);
            break;
        case Decl_Undefined:
            assert(!"referencing an undefined declaration");
        default:
            assert(!"unreachable");
            break;
        }
        break;
    }

    default:
        break;
    };

    ref.type = tovmt(val.type);
    return ref;
}

NkIrRef tupleIndex(NkIrRef ref, size_t i) {
    auto const tuple_t = ref.type;
    assert(i < tuple_t->as.tuple.elems.size);
    ref.type = tuple_t->as.tuple.elems.data[i].type;
    ref.post_offset += tuple_t->as.tuple.elems.data[i].offset;
    return ref;
}

NkIrRef arrayIndex(NkIrRef ref, size_t i) {
    auto const array_t = ref.type;
    assert(i < array_t->as.arr.elem_count);
    ref.type = array_t->as.arr.elem_type;
    ref.post_offset += nkt_sizeof(array_t->as.arr.elem_type) * i;
    return ref;
}

ValueInfo cast(nkltype_t type, ValueInfo val) {
    val.type = type;
    return val;
}

ValueInfo store(NklCompiler c, NkIrRef const &dst, ValueInfo src) {
    auto const dst_type = fromvmt(dst.type);
    auto const src_type = src.type;
    if (nklt_tclass(dst_type) == NklType_Slice && nklt_tclass(src_type) == NkType_Ptr &&
        nklt_tclass(src_type->as.ptr.target_type) == NkType_Array &&
        nklt_typeid(dst_type->as.slice.target_type) ==
            nklt_typeid(src_type->as.ptr.target_type->as.arr.elem_type)) {
        auto ptr_ref = tupleIndex(dst, 0);
        auto size_ref = tupleIndex(dst, 1);
        CHECK(store(c, ptr_ref, cast(nkl_get_ptr(dst_type->as.slice.target_type), src)));
        CHECK(store(
            c,
            size_ref,
            makeValue<uint64_t>(c, u64_t, src_type->as.ptr.target_type->as.arr.elem_count)));
        return makeRef(dst);
    }
    if (nklt_tclass(dst_type) == NklType_Any && nklt_tclass(src_type) == NkType_Ptr) {
        auto data_ref = tupleIndex(dst, 0);
        auto type_ref = tupleIndex(dst, 1);
        CHECK(store(c, data_ref, cast(void_ptr_t, src)));
        CHECK(store(c, type_ref, makeValue<nkltype_t>(c, typeref_t, src_type->as.ptr.target_type)));
        return makeRef(dst);
    }
    if (nklt_tclass(dst_type) == NklType_Union) {
        for (size_t i = 0; i < dst_type->as.strct.fields.size; i++) {
            if (nklt_typeid(dst_type->as.strct.fields.data[i].type) == nklt_typeid(src_type)) {
                // TODO Implement cast for refs
                auto dst_ref = dst;
                dst_ref.type = tovmt(src_type);
                return store(c, dst_ref, src);
            }
        }
    }
    if (nklt_typeid(dst_type) != nklt_typeid(src_type) &&
        !(nklt_tclass(dst_type) == NkType_Ptr && nklt_tclass(src_type) == NkType_Ptr &&
          nklt_tclass(src_type->as.ptr.target_type) == NkType_Array &&
          nklt_typeid(src_type->as.ptr.target_type->as.arr.elem_type) ==
              nklt_typeid(dst_type->as.ptr.target_type))) {
        return error(
                   c,
                   "cannot store value of type `%s` into a slot of type `%s`",
                   (char const *)[&]() {
                       auto sb = nksb_create();
                       nklt_inspect(src_type, sb);
                       return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                           nksb_free(sb);
                       });
                   }(),
                   (char const *)[&]() {
                       auto sb = nksb_create();
                       nklt_inspect(dst_type, sb);
                       return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                           nksb_free(sb);
                       });
                   }()),
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

ComptimeConst comptimeCompileNode(NklCompiler c, NklAstNode node, nkltype_t type = nullptr);
nklval_t comptimeCompileNodeGetValue(NklCompiler c, NklAstNode node, nkltype_t type = nullptr);
ValueInfo compile(NklCompiler c, NklAstNode node, nkltype_t type = nullptr);
Void compileStmt(NklCompiler c, NklAstNode node);

ValueInfo getStructIndex(NklCompiler c, ValueInfo const &lhs, nkltype_t struct_t, nkid name) {
    auto index = nklt_struct_index(struct_t, name);
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
    return makeRef(tupleIndex(asRef(c, lhs), index));
}

ValueInfo getUnionIndex(NklCompiler c, ValueInfo const &lhs, nkltype_t struct_t, nkid name) {
    // TODO  Boilerplate between getStructIndex and getUnionIndex
    auto index = nklt_struct_index(struct_t, name);
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
    return cast(struct_t->as.strct.fields.data[index].type, lhs);
}

ValueInfo deref(NklCompiler c, NkIrRef ref) {
    assert(nklt_tclass(fromvmt(ref.type)) == NkType_Ptr);
    auto const type = ref.type->as.ptr.target_type;
    if (ref.is_indirect) {
        DEFINE(
            val,
            store(c, nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, ref.type)), makeRef(ref)));
        ref = asRef(c, val);
    }
    ref.is_indirect = true;
    ref.type = type;
    return makeRef(ref);
}

ValueInfo getMember(NklCompiler c, ValueInfo const &lhs, nkid name) {
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

    case NklType_Struct:
        return getStructIndex(c, lhs, lhs.type, name);

    case NklType_Enum:
    case NklType_Slice:
    case NklType_Any:
        return getStructIndex(c, lhs, lhs.type->underlying_type, name);

    case NklType_Union:
        return getUnionIndex(c, lhs, lhs.type, name);

    case NkType_Ptr: {
        return getMember(c, deref(c, asRef(c, lhs)), name);
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
}

ValueInfo getIndex(NklCompiler c, ValueInfo const &lhs, ValueInfo const &index) {
    switch (nklt_tclass(lhs.type)) {
    case NkType_Array: {
        // TODO Optimize array indexing
        // TODO Think about type correctness in array indexing
        // TODO Using u8 to correctly index array in c
        auto const elem_t = lhs.type->as.arr.elem_type;
        auto const elem_ptr_t = nkl_get_ptr(elem_t);
        auto const data_ref = asRef(c, makeInstr(nkir_make_lea({}, asRef(c, lhs)), u8_ptr_t));
        auto const mul = nkir_make_mul(
            {},
            asRef(c, index),
            asRef(c, makeValue<uint64_t>(c, u64_t, nklt_sizeof(lhs.type->as.arr.elem_type))));
        auto const offset_ref = asRef(c, makeInstr(mul, u64_t));
        auto const add = nkir_make_add({}, data_ref, offset_ref);
        return deref(c, asRef(c, makeInstr(add, elem_ptr_t)));
    }

    case NkType_Tuple: {
        if (!isKnown(index)) {
            return error(c, "comptime value expected in tuple index"), ValueInfo{};
        }
        auto const i = nklval_as(uint64_t, asValue(c, index));
        if (i >= lhs.type->as.tuple.elems.size) {
            return error(c, "tuple index out of range"), ValueInfo{};
        }
        return makeRef(tupleIndex(asRef(c, lhs), i));
    }

    case NklType_Slice: {
        // TODO Using u8 to correctly index slice in c
        auto const elem_t = lhs.type->as.slice.target_type;
        auto const elem_ptr_t = nkl_get_ptr(lhs.type->as.slice.target_type);
        auto data_ref = asRef(c, lhs);
        data_ref.type = tovmt(u8_ptr_t);
        auto const mul = nkir_make_mul(
            {}, asRef(c, index), asRef(c, makeValue<uint64_t>(c, u64_t, nklt_sizeof(elem_t))));
        auto const offset_ref = asRef(c, makeInstr(mul, u64_t));
        auto const add = nkir_make_add({}, data_ref, offset_ref);
        return deref(c, asRef(c, makeInstr(add, elem_ptr_t)));
    }

    case NkType_Ptr: {
        return getIndex(c, deref(c, asRef(c, lhs)), index);
    }

    default: {
        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };
        nklt_inspect(lhs.type, sb);
        auto type_str = nksb_concat(sb);
        return error(c, "type `%.*s` is not indexable", type_str.size, type_str.data), ValueInfo{};
    }
    }
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
        DEFINE(index, compile(c, narg1(node)));
        if (index.type->tclass != NkType_Numeric) {
            return error(c, "expected number in index"), ValueInfo{};
        }
        return getIndex(c, lhs, index);
    } else if (node->id == n_deref) {
        DEFINE(arg, compile(c, narg0(node)));
        if (arg.type->tclass != NkType_Ptr) {
            return error(c, "pointer expected in dereference"), ValueInfo{};
        }
        return deref(c, asRef(c, arg));
    } else if (node->id == n_member) {
        DEFINE(const lhs, compile(c, narg0(node)));
        auto const name = s2nkid(narg1(node)->token->text);
        return getMember(c, lhs, name);
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

NkIrFunct nkl_compileFile(NklCompiler c, fs::path path, bool create_scope = true);

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
    nkltype_t args_t = nkl_get_tuple(c->arena, {params_types.data(), params_types.size()}, 1);

    auto fn_t = nkl_get_fn({ret_t, args_t, call_conv, is_variadic});

    auto fn = nkir_makeFunct(c->ir);
    auto pop_fn = pushFn(c, fn);

    std::string fn_name;
    if (c->node_stack.size() >= 2 &&
        (*(c->node_stack.rbegin() + 1)).node->id == n_comptime_const_def) {
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
        return {};
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
    nkltype_t args_t = nkl_get_tuple(c->arena, {params_types.data(), params_types.size()}, 1);

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
    case NkType_Void:
        if (init_nodes.size > 0) {
            return error(c, "too many values to init void");
        }
        break;
    default:
        NK_LOG_ERR("initFromAst is not implemented for this type");
        assert(!"unreachable");
        break;
    }
}

ValueInfo compileAndDiscard(NklCompiler c, NklAstNode node) {
    // TODO Boilerplate between comptimeCompileNode and compileAndDiscard
    auto fn = nkir_makeFunct(c->ir);
    auto pop_fn = pushFn(c, fn);
    nkir_startFunct(
        fn,
        cs2s("#comptime"),
        tovmt(nkl_get_fn(
            NkltFnInfo{void_t, nkl_get_tuple(c->arena, {nullptr, 0}, 1), NkCallConv_Nk, false})));
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), irBlockName(c, "start"));

    pushFnScope(c, fn);
    defer {
        popScope(c);
    };

    DEFINE(val, compile(c, node));

    nkir_discardFunct(fn);
    c->fn_scopes.erase(fn); // TODO Actually delete persistent scopes

    return val;
}

template <class F>
ValueInfo compileCompositeType(
    NklCompiler c,
    NklAstNode node,
    F const &create_type,
    std::unordered_map<nkid, ValueInfo> *out_inits = nullptr) {
    std::vector<NklField> fields{};

    auto nodes = nargs0(node);
    for (size_t i = 0; i < nodes.size; i++) {
        auto field_node = &nodes.data[i];
        nkid name = s2nkid(narg0(field_node)->token->text);
        DEFINE(type_val, comptimeCompileNodeGetValue(c, narg1(field_node)));
        if (nklval_tclass(type_val) != NklType_Typeref) {
            return error(c, "type expected in tuple type"), ValueInfo{};
        }
        auto type = nklval_as(nkltype_t, type_val);
        fields.emplace_back(NklField{
            .name = name,
            .type = type,
        });
        if (out_inits && narg2(field_node) && narg2(field_node)->id) {
            DEFINE(init_val, comptimeCompileNodeGetValue(c, narg2(field_node), type));
            (*out_inits)[name] = makeValue(c, init_val);
        }
    }

    return makeValue<nkltype_t>(c, typeref_t, create_type({fields.data(), fields.size()}));
}

ValueInfo import(NklCompiler c, fs::path filepath) {
    if (!fs::exists(filepath)) {
        auto const str = filepath.string();
        return error(c, "imported file `%.*s` doesn't exist", str.size(), str.c_str()), ValueInfo{};
    }
    filepath = fs::canonical(filepath);
    auto it = c->imports.find(filepath);
    if (it == c->imports.end()) {
        DEFINE(fn, nkl_compileFile(c, filepath));
        std::tie(it, std::ignore) = c->imports.emplace(filepath, fn);
    }
    auto fn = it->second;
    auto fn_val_info = makeValue<decltype(fn)>(c, fromvmt(nkir_functGetType(fn)), fn);
    auto fn_val = asValue(c, fn_val_info);
    auto stem = filepath.stem().string();
    CHECK(defineComptimeConst(
        c, s2nkid({stem.c_str(), stem.size()}), makeValueComptimeConst(fn_val)));
    return fn_val_info;
}

template <class T>
ValueInfo makeNumeric(NklCompiler c, nkltype_t type, nkstr str, char const *fmt) {
    T value = 0;
    // TODO Replace sscanf in compiler
    int res = std::sscanf(str.data, fmt, &value);
    (void)res;
    assert(res > 0 && res != EOF && "numeric constant parsing failed");
    return makeValue<T>(c, type, value);
}

ValueInfo compile(NklCompiler c, NklAstNode node, nkltype_t type) {
#ifdef BUILD_WITH_EASY_PROFILER
    auto block_name = std::string{"compile: "} + s_nkl_ast_node_names[node->id];
#endif // BUILD_WITH_EASY_PROFILER
    EASY_BLOCK(block_name.c_str(), ::profiler::colors::DeepPurple100);
    NK_LOG_DBG("node: %s", s_nkl_ast_node_names[node->id]);

    c->node_stack.emplace_back(NodeInfo{
        .node = node,
        .type = type,
    });
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

    case n_any_t: {
        return makeValue<nkltype_t>(c, typeref_t, any_t);
    }

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
            auto const ret_ref = nkir_makeRetRef(c->ir);
            DEFINE(arg, compile(c, narg0(node), fromvmt(ret_ref.type)));
            CHECK(store(c, ret_ref, arg));
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
        pushScope(c);
        defer {
            popScope(c);
        };
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
        DEFINE(lhs, compile(c, narg0(node), c->node_stack.back().type));                     \
        DEFINE(rhs, compile(c, narg1(node), lhs.type));                                      \
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
        DEFINE(rhs, compile(c, narg1(node), lhs.type));                                    \
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

        DEFINE(const src, compile(c, narg1(node)));
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
                return cast(dst_type, src);
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
                return cast(dst_type, src);
            }

            case NkType_Ptr: {
                return cast(dst_type, src);
            }
            }
            break;
        }

        case NkType_Fn: {
            switch (src_type->tclass) {
            case NkType_Fn: {
                return cast(dst_type, src);
            }
            }
            break;
        }

        case NkType_Void: {
            (void)asRef(c, src);
            return makeVoid();
        }
        }

        return error(
                   c,
                   "cannot cast value of type `%s` to type `%s`",
                   (char const *)[&]() {
                       auto sb = nksb_create();
                       nklt_inspect(src_type, sb);
                       return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                           nksb_free(sb);
                       });
                   }(),
                   (char const *)[&]() {
                       auto sb = nksb_create();
                       nklt_inspect(dst_type, sb);
                       return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                           nksb_free(sb);
                       });
                   }()),
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

    case n_tuple: {
        auto nodes = nargs0(node);
        std::vector<nkltype_t> types;
        std::vector<ValueInfo> values;
        values.reserve(nodes.size);
        for (size_t i = 0; i < nodes.size; i++) {
            APPEND(values, compile(c, &nodes.data[i]));
            types.emplace_back(values.back().type);
        }
        auto tuple_t = nkl_get_tuple(c->arena, {types.data(), types.size()}, 1);
        auto const ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(tuple_t)));
        if (values.size() != tuple_t->as.tuple.elems.size) {
            return error(c, "invalid number of values in tuple literal"), ValueInfo{};
        }
        for (size_t i = 0; i < values.size(); i++) {
            CHECK(store(c, tupleIndex(ref, i), values[i]));
        }
        return makeRef(ref);
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
            c, typeref_t, nkl_get_tuple(c->arena, {types.data(), types.size()}, 1));
    }

    case n_import: {
        auto const name = narg0(node)->token->text;
        std::string const filename = std_str(name) + ".nkl";
        auto stdlib_path = fs::path{c->stdlib_dir};
        if (!stdlib_path.is_absolute()) {
            stdlib_path = fs::path{c->compiler_dir} / stdlib_path;
        }
        auto const filepath = (stdlib_path / filename).lexically_normal();
        return import(c, filepath);
    }

    case n_import_path: {
        nkstr const text{node->token->text.data + 1, node->token->text.size - 2};
        auto const name = text;
        auto filepath = fs::path(std_view(name)).lexically_normal();
        if (!filepath.is_absolute()) {
            filepath = (c->file_stack.top().parent_path() / filepath).lexically_normal();
        }
        return import(c, filepath);
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

    case n_intrinsic: {
        nkstr name_str = node->token->text;
        nkid name = s2nkid(name_str);
        if (name == cs2nkid("@typeof")) {
            return error(c, "invalid use of `%.*s` intrinsic", name_str.size, name_str.data),
                   ValueInfo{};
        } else {
            return error(c, "invalid intrinsic `%.*s`", name_str.size, name_str.data), ValueInfo{};
        }
    }

    case n_float: {
        NkNumericValueType value_type =
            (c->node_stack.back().type && nklt_tclass(c->node_stack.back().type) == NkType_Numeric)
                ? c->node_stack.back().type->as.num.value_type
                : Float64;
        auto const text = node->token->text;
        switch (value_type) {
        case Float32:
            return makeNumeric<float>(c, f32_t, text, "%" SCNf32);
        default:
        case Float64:
            return makeNumeric<double>(c, f64_t, text, "%" SCNf64);
        }
    }

    case n_int: {
        NkNumericValueType value_type =
            (c->node_stack.back().type && nklt_tclass(c->node_stack.back().type) == NkType_Numeric)
                ? c->node_stack.back().type->as.num.value_type
                : Int64;
        auto const text = node->token->text;
#define X(NAME, VALUE_TYPE, CTYPE) \
    case VALUE_TYPE:               \
        return makeNumeric<CTYPE>(c, CAT(NAME, _t), text, "%" CAT(SCN, NAME));
        switch (value_type) { NUMERIC_ITERATE(X) }
        assert(!"unreachable");
        return {};
#undef X
    }

    case n_string: {
        nkstr const text{node->token->text.data + 1, node->token->text.size - 2};

        auto ar_t = nkl_get_array(i8_t, text.size + 1);
        auto str_t = nkl_get_ptr(ar_t);

        auto str = (char *)nk_allocate(c->arena, text.size + 1);
        std::memcpy(str, text.data, text.size);
        str[text.size] = '\0';

        return makeValue<void *>(c, str_t, (void *)str);
    }

    case n_escaped_string: {
        nkstr const text{node->token->text.data + 1, node->token->text.size - 2};

        auto sb = nksb_create();
        defer {
            nksb_free(sb);
        };
        nksb_str_unescape(sb, text);
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
        std::unordered_map<nkid, ValueInfo> inits;
        DEFINE(
            type_val,
            compileCompositeType(
                c,
                node,
                [c](NklFieldArray fields) {
                    return nkl_get_struct(c->arena, fields);
                },
                &inits));
        if (!inits.empty()) {
            c->struct_inits[nklt_typeid(nklval_as(nkltype_t, asValue(c, type_val)))] =
                std::move(inits);
        }
        return type_val;
    }

    case n_union: {
        return compileCompositeType(c, node, [c](NklFieldArray fields) {
            return nkl_get_union(c->arena, fields);
        });
    }

    case n_enum: {
        return compileCompositeType(c, node, [c](NklFieldArray fields) {
            return nkl_get_enum(c->arena, fields);
        });
    };

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
        if (narg0(node)->id == n_intrinsic) {
            nkid name = s2nkid(narg0(node)->token->text);
            if (name == cs2nkid("@typeof")) {
                auto arg_nodes = nargs1(node);
                if (arg_nodes.size != 1) {
                    return error(c, "@typeof expects exactly one argument"), ValueInfo{};
                }
                DEFINE(val, compileAndDiscard(c, narg1(&arg_nodes.data[0])));
                return makeValue<nkltype_t>(c, typeref_t, val.type);
            } else if (name == cs2nkid("@sizeof")) {
                auto arg_nodes = nargs1(node);
                if (arg_nodes.size != 1) {
                    return error(c, "@sizeof expects exactly one argument"), ValueInfo{};
                }
                DEFINE(val, compileAndDiscard(c, narg1(&arg_nodes.data[0])));
                return makeValue<uint64_t>(c, u64_t, nklt_sizeof(val.type));
            }
        }

        DEFINE(lhs, compile(c, narg0(node)));

        if (lhs.type->tclass != NkType_Fn) {
            return error(c, "function expected in call"), ValueInfo{};
        }

        auto const fn_t = lhs.type;

        auto arg_nodes = nargs1(node);

        if (fn_t->as.fn.is_variadic) {
            if (arg_nodes.size < fn_t->as.fn.args_t->as.tuple.elems.size) {
                return error(
                           c,
                           "invalid number of arguments, expected at least `%lu`, provided `%lu`",
                           fn_t->as.fn.args_t->as.tuple.elems.size,
                           arg_nodes.size),
                       ValueInfo{};
            }
        } else {
            if (arg_nodes.size != fn_t->as.fn.args_t->as.tuple.elems.size) {
                return error(
                           c,
                           "invalid number of arguments, expected `%lu`, provided `%lu`",
                           fn_t->as.fn.args_t->as.tuple.elems.size,
                           arg_nodes.size),
                       ValueInfo{};
            }
        }

        std::vector<ValueInfo> args_info{};
        std::vector<nkltype_t> args_types{};

        args_info.reserve(arg_nodes.size);
        args_types.reserve(arg_nodes.size);

        for (size_t i = 0; i < arg_nodes.size; i++) {
            auto param_t = i < fn_t->as.fn.args_t->as.tuple.elems.size
                               ? fn_t->as.fn.args_t->as.tuple.elems.data[i].type
                               : nullptr;
            // TODO Support named args in call
            DEFINE(val_info, compile(c, narg1(&arg_nodes.data[i]), param_t));
            args_info.emplace_back(val_info);
            args_types.emplace_back(param_t ? param_t : val_info.type);
        }

        auto args_t = nkl_get_tuple(c->arena, {args_types.data(), args_types.size()}, 1);

        NkIrRef args_ref{};
        if (nklt_sizeof(args_t)) {
            args_ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(args_t)));
        }

        for (size_t i = 0; i < args_info.size(); i++) {
            CHECK(store(c, tupleIndex(args_ref, i), args_info[i]));
        }

        return makeInstr(nkir_make_call({}, asRef(c, lhs), args_ref), fn_t->as.fn.ret_t);
    }

    case n_object_literal: {
        DEFINE(type_val, comptimeCompileNodeGetValue(c, narg0(node)));

        if (nklval_tclass(type_val) != NklType_Typeref) {
            return error(c, "type expected in object literal"), ValueInfo{};
        }

        auto const type = nklval_as(nkltype_t, type_val);

        auto const init_nodes = nargs1(node);

        // TODO Support compile time object literals

        switch (nklt_tclass(type)) {
        case NklType_Struct: {
            auto const value_count = init_nodes.size;
            std::vector<nkid> names;
            std::vector<ValueInfo> values;
            names.reserve(value_count);
            values.reserve(value_count);
            // TODO bool all_known = true;
            for (size_t i = 0; i < value_count; i++) {
                auto const init_node = &init_nodes.data[i];
                auto const name_node = narg0(init_node);
                // TODO Some boilerplate with node_stack
                if (name_node && name_node->id) {
                    c->node_stack.emplace_back(NodeInfo{
                        .node = name_node,
                        .type = nullptr,
                    });
                }
                defer {
                    if (name_node && name_node->id) {
                        c->node_stack.pop_back();
                    }
                };
                auto const name = (name_node && name_node->id) ? s2nkid(name_node->token->text) : 0;
                if (name &&
                    std::find(std::begin(names), std::end(names), name) != std::end(names)) {
                    return error(c, "duplicate names"), ValueInfo{};
                }
                if (name && nklt_struct_index(type, name) == -1lu) {
                    return error(
                               c,
                               "no field named `%s` in type `struct`",
                               nkid2cs(name),
                               (char const *)[&]() {
                                   auto sb = nksb_create();
                                   nklt_inspect(type, sb);
                                   return makeDeferrerWithData(nksb_concat(sb).data, [sb]() {
                                       nksb_free(sb);
                                   });
                               }()),
                           ValueInfo{};
                }
                names.emplace_back(name);
                APPEND(
                    values,
                    compile(
                        c, narg1(init_node), type->underlying_type->as.tuple.elems.data[i].type));
                // TODO if (!isKnown(values.back())) {
                //     all_known = false;
                // }
            }
            auto const ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(type)));
            auto const fields = type->as.strct.fields;
            std::vector<bool> initted;
            initted.resize(fields.size);
            size_t last_positional = 0;
            for (size_t vi = 0; vi < value_count; vi++) {
                auto const init_node = &init_nodes.data[vi];
                auto const name_node = narg0(init_node);
                // TODO Some boilerplate with node_stack
                if (name_node && name_node->id) {
                    c->node_stack.emplace_back(NodeInfo{
                        .node = name_node,
                        .type = nullptr,
                    });
                }
                defer {
                    if (name_node && name_node->id) {
                        c->node_stack.pop_back();
                    }
                };
                if (vi >= fields.size) {
                    return error(c, "too many values"), ValueInfo{};
                }
                auto fi = names[vi] ? nklt_struct_index(type, names[vi]) : vi;
                if (names[vi]) {
                    if (fi < last_positional) {
                        return error(c, "named argument conflicts with positional"), ValueInfo{};
                    }
                } else {
                    last_positional++;
                }
                CHECK(store(c, tupleIndex(ref, fi), values[vi]));
                initted.at(fi) = true;
            }
            auto const it = c->struct_inits.find(nklt_typeid(type));
            if (it != c->struct_inits.end()) {
                auto const &inits = it->second;
                for (size_t fi = 0; fi < fields.size; fi++) {
                    if (!initted[fi]) {
                        auto it = inits.find(fields.data[fi].name);
                        if (it != inits.end()) {
                            CHECK(store(c, tupleIndex(ref, fi), it->second));
                            initted.at(fi) = true;
                        }
                    }
                }
            }
            if (!std::all_of(std::begin(initted), std::end(initted), [](bool x) {
                    return x;
                })) {
                return error(c, "not all fields are initialized"), ValueInfo{};
            }
            return makeRef(ref);
        }

        case NkType_Tuple:
        case NklType_Any:
        case NklType_Slice: {
            auto const value_count = init_nodes.size;
            std::vector<nkid> names;
            std::vector<ValueInfo> values;
            names.reserve(value_count);
            values.reserve(value_count);
            // TODO bool all_known = true;
            for (size_t i = 0; i < value_count; i++) {
                auto const init_node = &init_nodes.data[i];
                auto const name_node = narg0(init_node);
                names.emplace_back(
                    (name_node && name_node->id) ? s2nkid(name_node->token->text) : 0);
                APPEND(values, compile(c, narg1(init_node), type->as.tuple.elems.data[i].type));
                // TODO if (!isKnown(values.back())) {
                //     all_known = false;
                // }
            }
            auto const tuple_t = type;
            auto const ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(type)));
            if (value_count != tuple_t->as.tuple.elems.size) {
                return error(c, "invalid number of values in composite literal"), ValueInfo{};
            }
            for (size_t i = 0; i < value_count; i++) {
                CHECK(store(c, tupleIndex(ref, i), values[i]));
            }
            return makeRef(ref);
        }

        case NkType_Array: {
            auto const value_count = init_nodes.size;
            if (value_count != type->as.arr.elem_count) {
                return error(c, "invalid number of values in array literal"), ValueInfo{};
            }
            std::vector<nkid> names;
            std::vector<ValueInfo> values;
            names.reserve(value_count);
            values.reserve(value_count);
            // TODO bool all_known = true;
            for (size_t i = 0; i < value_count; i++) {
                auto const init_node = &init_nodes.data[i];
                auto const name_node = narg0(init_node);
                names.emplace_back(
                    (name_node && name_node->id) ? s2nkid(name_node->token->text) : 0);
                APPEND(values, compile(c, narg1(init_node), type->as.arr.elem_type));
                // TODO if (!isKnown(values.back())) {
                //     all_known = false;
                // }
            }
            auto const ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(type)));
            for (size_t i = 0; i < value_count; i++) {
                CHECK(store(c, arrayIndex(ref, i), values[i]));
            }
            return makeRef(ref);
        }

        case NklType_Enum: {
            if (init_nodes.size != 1) {
                return error(c, "single value expected in enum literal"), ValueInfo{};
            }
            auto const init_node = &init_nodes.data[0];
            auto const name_node = narg0(init_node);
            if (!name_node || !name_node->id) {
                return error(c, "name expected in enum literal"), ValueInfo{};
            }
            auto const name = s2nkid(name_node->token->text);
            auto const struct_t = type->underlying_type;
            auto const union_t = struct_t->as.strct.fields.data[0].type;
            // TODO Can we not create a local variable undonditionally in enum literal?
            auto const enum_ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(type)));
            auto const data_ref = tupleIndex(enum_ref, 0);
            auto const tag_ref = tupleIndex(enum_ref, 1);
            DEFINE(field_ref, getUnionIndex(c, makeRef(data_ref), union_t, name));
            DEFINE(value, compile(c, narg1(init_node), field_ref.type));
            CHECK(store(c, asRef(c, field_ref), value));
            // TODO Indexing union twice in enum literal
            auto const index = nklt_struct_index(union_t, name);
            CHECK(store(c, tag_ref, makeValue<uint64_t>(c, u64_t, index)));
            return makeRef(enum_ref);
        }

        default: {
            nklval_t val{nk_allocate(c->arena, nklt_sizeof(type)), type};
            std::memset(nklval_data(val), 0, nklval_sizeof(val));

            // TODO Ignoring named args in object literal
            std::vector<NklAstNode_T> nodes;
            nodes.reserve(init_nodes.size);
            for (size_t i = 0; i < init_nodes.size; i++) {
                nodes.emplace_back(narg1(&init_nodes.data[i])[0]);
            }

            CHECK(initFromAst(c, val, {nodes.data(), nodes.size()}));

            return makeValue(c, val);
        }
        }
    }

    case n_assign: {
        std::vector<ValueInfo> lhss;
        auto lhs_nodes = nargs0(node);
        lhss.reserve(lhs_nodes.size);
        for (size_t i = 0; i < lhs_nodes.size; i++) {
            APPEND(lhss, getLvalueRef(c, &lhs_nodes.data[i]));
        }
        DEFINE(rhs, compile(c, narg1(node), lhss.size() == 1 ? lhss[0].type : nullptr));
        if (lhss.size() == 1) {
            return store(c, asRef(c, lhss[0]), rhs);
        } else {
            if (nklt_tclass(rhs.type) != NkType_Tuple) {
                return error(c, "tuple expected in multiple assignment"), ValueInfo{};
            }
            if (rhs.type->as.tuple.elems.size != lhss.size()) {
                return error(c, "invalid number of values in multiple assignment"), ValueInfo{};
            }
            for (size_t i = 0; i < lhss.size(); i++) {
                auto rhs_ref = asRef(c, rhs);
                store(c, asRef(c, lhss[i]), makeRef(tupleIndex(rhs_ref, i)));
            }
            return makeVoid();
        }
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
            DEFINE(val, compile(c, narg2(node), type));
            CHECK(store(c, ref, val));
        }
        return makeVoid();
    }

    default:
        return error(c, "unknown ast node"), ValueInfo{};
    }
}

ComptimeConst comptimeCompileNode(NklCompiler c, NklAstNode node, nkltype_t type) {
    auto fn = nkir_makeFunct(c->ir);
    NkltFnInfo fn_info{nullptr, nkl_get_tuple(c->arena, {nullptr, 0}, 1), NkCallConv_Nk, false};

    auto pop_fn = pushFn(c, fn);

    auto const vm_fn_info = tovmf(fn_info);
    nkir_startIncompleteFunct(fn, cs2s("#comptime"), &vm_fn_info);
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), irBlockName(c, "start"));

    pushFnScope(c, fn);
    defer {
        popScope(c);
    };

    DEFINE(cnst_val, compile(c, node, type));
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

nklval_t comptimeCompileNodeGetValue(NklCompiler c, NklAstNode node, nkltype_t type) {
    DEFINE(cnst, comptimeCompileNode(c, node, type));
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

NkIrFunct nkl_compile(NklCompiler c, NklAstNode root, bool create_scope = true) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);

    auto fn = nkir_makeFunct(c->ir);
    auto pop_fn = pushFn(c, fn);

    auto top_level_fn_t =
        nkl_get_fn({void_t, nkl_get_tuple(c->arena, {nullptr, 0}, 1), NkCallConv_Nk, false});

    nkir_startFunct(fn, cs2s("#top_level"), tovmt(top_level_fn_t));
    nkir_startBlock(c->ir, nkir_makeBlock(c->ir), irBlockName(c, "start"));

    if (create_scope) {
        pushTopLevelFnScope(c, fn);
    }
    defer {
        if (create_scope) {
            popScope(c);
        }
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
    if (c->error_reported) {
        return;
    }

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
    c->error_reported = true;
}

void printError(NklCompiler c, char const *fmt, ...) {
    if (c->error_reported) {
        return;
    }

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
    c->error_reported = true;
}

NkIrFunct nkl_compileSrc(NklCompiler c, nkstr src, bool create_scope = true) {
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

    auto fn = nkl_compile(c, root, create_scope);
    if (c->error_occurred) {
        printError(c, c->err_token, err_str);
        return {};
    }

    return fn;
}

NkIrFunct nkl_compileFile(NklCompiler c, fs::path path, bool create_scope) {
    EASY_FUNCTION(::profiler::colors::DeepPurple100);
    NK_LOG_TRC(__func__);

    if (!fs::exists(path)) {
        auto const path_str = path.string();
        printError(c, "file `%.*s` doesn't exist", (int)path_str.size(), path_str.c_str());
        return {};
    }
    path = fs::canonical(path);

    c->file_stack.emplace(fs::relative(path));
    defer {
        c->file_stack.pop();
    };

    std::ifstream file{path};
    if (file) {
        std::string src{std::istreambuf_iterator<char>{file}, {}};
        return nkl_compileSrc(c, {src.data(), src.size()}, create_scope);
    } else {
        auto const path_str = path.string();
        printError(c, "failed to open file `%.*s`", (int)path_str.size(), path_str.c_str());
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
    std::vector<NklField> fields;
    fields.reserve(fields_raw.size());
    for (auto const &field : fields_raw) {
        fields.emplace_back(NklField{
            // TODO Treating slice as cstring, while we include excess zero charater
            .name = cs2nkid(field.name.data),
            .type = field.type,
        });
    }
    return nkl_get_struct(c->arena, {fields.data(), fields.size()});
}

NklCompiler nkl_compiler_create() {
    nkl_ast_init();

    auto arena = nk_create_arena();

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

    any_t = nkl_get_any(arena);
    typeref_t = nkl_get_typeref();
    void_t = nkl_get_void();
    void_ptr_t = nkl_get_ptr(void_t);
    bool_t = u8_t; // Modeling bool as u8
    u8_ptr_t = nkl_get_ptr(u8_t);

    return new (nk_allocate(nk_default_allocator, sizeof(NklCompiler_T))) NklCompiler_T{
        .ir = nkir_createProgram(),
        .arena = arena,
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

    pushScope(c);
    auto preload_filepath = fs::path{c->compiler_dir} / "preload.nkl";
    if (!fs::exists(preload_filepath)) {
        auto const path_str = preload_filepath.string();
        printError(c, "preload file `%.*s` doesn't exist", (int)path_str.size(), path_str.c_str());
        return false;
    }
    if (!nkl_compileFile(c, preload_filepath, false)) {
        return false;
    }

    auto config_filepath = fs::path{c->compiler_dir} / "config.nkl";
    if (!fs::exists(config_filepath)) {
        auto const path_str = config_filepath.string();
        printError(c, "config file `%.*s` doesn't exist", (int)path_str.size(), path_str.c_str());
        return false;
    }
    auto fn = nkl_compileFile(c, config_filepath);
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

    DEFINE(fn, nkl_compileFile(c, fs::path{std_view(path)}));
    nkir_invoke({&fn, nkir_functGetType(fn)}, {}, {});
    return true;
}
