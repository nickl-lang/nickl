#include "nkl/lang/compiler.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <map>
#include <optional>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast_impl.h"
#include "lexer.hpp"
#include "nk/vm/common.h"
#include "nk/vm/ir.h"
#include "nk/vm/ir_compile.h"
#include "nk/vm/value.h"
#include "nkl/lang/ast.h"
#include "nkl/lang/token.h"
#include "nkl/lang/value.h"
#include "ntk/allocator.h"
#include "ntk/arena.h"
#include "ntk/atom.h"
#include "ntk/common.h"
#include "ntk/file.h"
#include "ntk/log.h"
#include "ntk/profiler.h"
#include "ntk/string.h"
#include "ntk/string_builder.h"
#include "ntk/term.h"
#include "ntk/utils.h"
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

#ifndef SCNx8
#define SCNx8 "hhx"
#endif // SCNx8

#define SCNf32 "f"
#define SCNf64 "lf"

#define SCNXi8 SCNx8
#define SCNXu8 SCNx8
#define SCNXi16 SCNx16
#define SCNXu16 SCNx16
#define SCNXi32 SCNx32
#define SCNXu32 SCNx32
#define SCNXi64 SCNx64
#define SCNXu64 SCNx64

namespace {

#define X(TYPE, VALUE_TYPE) nkltype_t NK_CAT(TYPE, _t);
NUMERIC_ITERATE(X)
#undef X

nkltype_t any_t;
nkltype_t typeref_t;
nkltype_t void_t;
nkltype_t void_ptr_t;
nkltype_t bool_t;
nkltype_t u8_ptr_t;

struct Void {};

namespace fs = std::filesystem;

static bool fs_fileExists(fs::path const &path) {
    NK_PROF_FUNC();
    return fs::exists(path);
}

static fs::path fs_pathCanonical(fs::path const &path) {
    NK_PROF_FUNC();
    return fs::canonical(path);
}

static fs::path fs_pathRelative(fs::path const &path) {
    NK_PROF_FUNC();
    return fs::relative(path);
}

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
            NkIrFunct fn;
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
            usize index;
            nkltype_t type;
            NkIrFunct fn;
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
    NkIrFunct cur_fn;
    std::unordered_map<NkAtom, Decl> locals{};
};

static thread_local NklCompiler s_compiler;

struct TagInfo {
    NkAtom name;
    nklval_t val;
};

typedef NkSlice(TagInfo const) TagInfoArray;

struct NodeInfo {
    NklAstNode node;
    nkltype_t type{};
    TagInfoArray tags{};
};

} // namespace

struct NklCompiler_T {
    NkIrProg ir;
    NkArena arena;
    NkAllocator alloc{};

    std::string err_str{};
    NklTokenRef err_token{};
    bool error_occurred = false;
    bool error_reported = false;

    std::string compiler_dir{};
    std::string corelib_dir{};
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
    usize fn_counter{};

    std::unordered_map<NkAtom, usize> id2blocknum{};

    std::stack<fs::path> file_stack{};
    std::stack<NkString> src_stack{};

    std::unordered_map<nkl_typeid_t, std::unordered_map<NkAtom, ValueInfo>> struct_inits{};
};

namespace {

struct LinkTag {
    NkString libname;
    NkString prefix;
};

struct ExternTag {
    NkString abi;
};

NK_PRINTF_LIKE(2) void error(NklCompiler c, char const *fmt, ...) {
    nk_assert(!c->error_occurred && "compiler error already initialized");

    va_list ap;
    va_start(ap, fmt);
    NkStringBuilder sb{};
    nksb_vprintf(&sb, fmt, ap);
    c->err_str = nk_s2stdStr({NKS_INIT(sb)});
    nksb_free(&sb);
    va_end(ap);

    if (!c->node_stack.empty()) {
        c->err_token = c->node_stack.back().node->token;
    }

    c->error_occurred = true;
}

NkString irBlockName(NklCompiler c, char const *name) {
    // TODO Not ideal string management in compiler
    auto id = nk_cs2atom(name);
    auto num = c->id2blocknum[id]++;
    char buf[1024];
    usize size = std::snprintf(buf, NK_ARRAY_COUNT(buf), "%s%zu", name, num);

    auto str = nk_arena_allocT<char>(&c->arena, size + 1);
    std::memcpy(str, buf, size);
    str[size] = '\0';

    return {str, size};
}

Scope &curScope(NklCompiler c) {
    nk_assert(!c->scopes.empty() && "no current scope");
    return *c->scopes.back();
}

void pushScope(NklCompiler c) {
    NK_PROF_FUNC();
    NkIrFunct cur_fn = c->scopes.size() ? c->scopes.back()->cur_fn : nullptr;
    NK_LOG_DBG("entering scope=%zu", c->scopes.size());
    auto scope = &c->nonpersistent_scope_stack.emplace(Scope{false, cur_fn});
    c->scopes.emplace_back(scope);
}

void pushFnScope(NklCompiler c, NkIrFunct fn) {
    NK_PROF_FUNC();
    NK_LOG_DBG("entering persistent scope=%zu", c->scopes.size());
    auto scope = &c->persistent_scopes.emplace_back(Scope{false, fn});
    c->scopes.emplace_back(scope);
    c->fn_scopes.emplace(fn, &curScope(c));
}

void pushTopLevelFnScope(NklCompiler c, NkIrFunct fn) {
    NK_PROF_FUNC();
    NK_LOG_DBG("entering top level scope=%zu", c->scopes.size());
    auto scope = &c->persistent_scopes.emplace_back(Scope{true, fn});
    c->scopes.emplace_back(scope);
    c->fn_scopes.emplace(fn, &curScope(c));
}

void popScope(NklCompiler c) {
    NK_PROF_FUNC();
    nk_assert(!c->scopes.empty() && "popping an empty scope stack");
    if (!c->nonpersistent_scope_stack.empty()) {
        auto scope = &c->nonpersistent_scope_stack.top();
        if (scope == c->scopes.back()) {
            c->nonpersistent_scope_stack.pop();
        }
    }
    c->scopes.pop_back();
    NK_LOG_DBG("exiting scope=%zu", c->scopes.size());
}

void gen(NklCompiler c, NkIrInstr const &instr) {
    NK_LOG_DBG("gen: %s", s_nk_ir_names[instr.code]);
    nkir_gen(c->ir, instr);
}

ValueInfo makeVoid() {
    return {{}, void_t, v_none};
}

Decl &makeDecl(NklCompiler c, NkAtom name) {
    auto &locals = curScope(c).locals;

    NK_LOG_DBG("making declaration name=`" NKS_FMT "` scope=%zu", NKS_ARG(nk_atom2s(name)), c->scopes.size() - 1);

    auto it = locals.find(name);
    if (it != locals.end()) {
        NkString name_str = nk_atom2s(name);
        static Decl s_dummy_decl{};
        return error(c, "`" NKS_FMT "` is already defined", NKS_ARG(name_str)), s_dummy_decl;
    } else {
        std::tie(it, std::ignore) = locals.emplace(name, Decl{});
    }

    return it->second;
}

void defineComptimeConst(NklCompiler c, NkAtom name, ComptimeConst cnst) {
    NK_LOG_DBG("defining comptime const `" NKS_FMT "`", NKS_ARG(nk_atom2s(name)));
    makeDecl(c, name) = {{.comptime_const{cnst}}, Decl_ComptimeConst};
}

void defineLocal(NklCompiler c, NkAtom name, NkIrLocalVarId id, nkltype_t type) {
    NK_LOG_DBG("defining local `" NKS_FMT "`", NKS_ARG(nk_atom2s(name)));
    makeDecl(c, name) = {{.local{id, type, curScope(c).cur_fn}}, Decl_Local};
}

void defineGlobal(NklCompiler c, NkAtom name, NkIrGlobalVarId id, nkltype_t type) {
    NK_LOG_DBG("defining global `" NKS_FMT "`", NKS_ARG(nk_atom2s(name)));
    makeDecl(c, name) = {{.global{id, type}}, Decl_Global};
}

void defineExtSym(NklCompiler c, NkAtom name, NkIrExtSymId id, nkltype_t type) {
    NK_LOG_DBG("defining ext sym `" NKS_FMT "`", NKS_ARG(nk_atom2s(name)));
    makeDecl(c, name) = {{.ext_sym{.id = id, .type = type}}, Decl_ExtSym};
}

void defineArg(NklCompiler c, NkAtom name, usize index, nkltype_t type) {
    NK_LOG_DBG("defining arg `" NKS_FMT "`", NKS_ARG(nk_atom2s(name)));
    makeDecl(c, name) = {{.arg{index, type, curScope(c).cur_fn}}, Decl_Arg};
}

// TODO Restrict resolving local through stack frame boundaries
Decl &resolve(NklCompiler c, NkAtom name) {
    NK_LOG_DBG("resolving name=`" NKS_FMT "` scope=%zu", NKS_ARG(nk_atom2s(name)), c->scopes.size() - 1);

    for (usize i = c->scopes.size(); i > 0; i--) {
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
        {.cnst = nkir_makeConst(c->ir, {new (nk_arena_allocT<T>(&c->arena)) T{args...}, tovmt(type)})}, type, v_val};
}

ValueInfo makeValue(NklCompiler c, nklval_t val) {
    return {{.cnst = nkir_makeConst(c->ir, tovmv(val))}, nklval_typeof(val), v_val};
}

ValueInfo makeValue(NklCompiler c, nkltype_t type) {
    return {{.cnst = nkir_makeConst(c->ir, {nk_arena_alloc(&c->arena, nklt_sizeof(type)), tovmt(type)})}, type, v_val};
}

ValueInfo makeInstr(NkIrInstr const &instr, nkltype_t type) {
    return {{.instr{instr}}, type, v_instr};
}

ValueInfo makeRef(NkIrRef const &ref) {
    return {{.ref = ref}, fromvmt(ref.type), v_ref};
}

nklval_t comptimeConstGetValue(NklCompiler c, ComptimeConst &cnst) {
    NK_PROF_FUNC();

    switch (cnst.kind) {
        case ComptimeConst_Value:
            NK_LOG_DBG("returning comptime const as value");
            return cnst.value;
        case ComptimeConst_Funct: {
            NK_LOG_DBG("getting comptime const from funct");
            auto fn_t = nkir_functGetType(cnst.funct);
            auto type = fromvmt(fn_t->as.fn.ret_t);
            nklval_t val{nk_arena_alloc(&c->arena, nklt_sizeof(type)), type};
            nkir_invoke({&cnst.funct, fn_t}, tovmv(val), {});
            cnst = makeValueComptimeConst(val);
            return val;
        }
        default:
            nk_assert(!"unreachable");
            return {};
    }
}

bool isKnown(ValueInfo const &val) {
    return val.kind == v_val || (val.kind == v_decl && val.as.decl->kind == Decl_ComptimeConst);
}

nklval_t asValue(NklCompiler c, ValueInfo const &val) {
    nk_assert(isKnown(val) && "getting unknown value");
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
            nk_assert(!"unreachable");
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
            if (dst.ref_type == NkIrRef_None && nklt_sizeof(val.type)) {
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
                        c->ir, nkir_makeConst(c->ir, tovmv(comptimeConstGetValue(c, decl.as.comptime_const))));
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
                    nk_assert(!"referencing an undefined declaration");
                default:
                    nk_assert(!"unreachable");
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

NkIrRef tupleIndex(NkIrRef ref, nkltype_t tuple_t, usize i) {
    nk_assert(i < tuple_t->as.tuple.elems.size);
    ref.type = tovmt(tuple_t->as.tuple.elems.data[i].type);
    ref.post_offset += tuple_t->as.tuple.elems.data[i].offset;
    return ref;
}

NkIrRef arrayIndex(NkIrRef ref, nkltype_t array_t, usize i) {
    nk_assert(i < array_t->as.arr.elem_count);
    ref.type = tovmt(array_t->as.arr.elem_type);
    ref.post_offset += nklt_sizeof(array_t->as.arr.elem_type) * i;
    return ref;
}

ValueInfo cast(nkltype_t type, ValueInfo val) {
    val.type = type;
    return val;
}

Void comptimeStore(NklCompiler c, nklval_t dst, nklval_t src) {
    auto const dst_type = nklval_typeof(dst);
    auto const src_type = nklval_typeof(src);
    if (nklt_tclass(dst_type) == NklType_Slice && nklt_tclass(src_type) == NkType_Ptr &&
        nklt_tclass(src_type->as.ptr.target_type) == NkType_Array &&
        nklt_typeid(dst_type->as.slice.target_type) == nklt_typeid(src_type->as.ptr.target_type->as.arr.elem_type)) {
        auto data_v = nklval_tuple_at(dst, 0);
        auto size_v = nklval_tuple_at(dst, 1);
        CHECK(comptimeStore(c, data_v, nklval_reinterpret_cast(nkl_get_ptr(dst_type->as.slice.target_type), src)));
        CHECK(comptimeStore(c, size_v, {(void *)&src_type->as.ptr.target_type->as.arr.elem_count, u64_t}));
    } else if (nklt_tclass(dst_type) == NklType_Any && nklt_tclass(src_type) == NkType_Ptr) {
        auto data_v = nklval_tuple_at(dst, 0);
        auto type_v = nklval_tuple_at(dst, 1);
        CHECK(comptimeStore(c, data_v, nklval_reinterpret_cast(void_ptr_t, src)));
        CHECK(comptimeStore(c, type_v, {(void *)&src_type->as.ptr.target_type, typeref_t}));
    } else if (nklt_tclass(dst_type) == NklType_Union) {
        for (usize i = 0; i < dst_type->as.strct.fields.size; i++) {
            if (nklt_typeid(dst_type->as.strct.fields.data[i].type) == nklt_typeid(src_type)) {
                return comptimeStore(c, nklval_reinterpret_cast(src_type, dst), src);
            }
            nk_assert(!"invalid store");
        }
    } else {
        nk_assert(
            !(nklt_typeid(dst_type) != nklt_typeid(src_type) &&
              !(nklt_tclass(dst_type) == NkType_Ptr && nklt_tclass(src_type) == NkType_Ptr &&
                nklt_tclass(src_type->as.ptr.target_type) == NkType_Array &&
                nklt_typeid(src_type->as.ptr.target_type->as.arr.elem_type) ==
                    nklt_typeid(dst_type->as.ptr.target_type))) &&
            "invalid store");
        nklval_copy(nklval_data(dst), src);
    }
    return {};
}

ValueInfo store(NklCompiler c, NkIrRef const &dst, ValueInfo src) {
    auto const dst_type = fromvmt(dst.type);
    auto const src_type = src.type;
    if (nklt_tclass(dst_type) == NklType_Slice && nklt_tclass(src_type) == NkType_Ptr &&
        nklt_tclass(src_type->as.ptr.target_type) == NkType_Array &&
        nklt_typeid(dst_type->as.slice.target_type) == nklt_typeid(src_type->as.ptr.target_type->as.arr.elem_type)) {
        auto const struct_t = dst_type->underlying_type;
        auto const tuple_t = struct_t->underlying_type;
        auto data_ref = tupleIndex(dst, tuple_t, 0);
        auto size_ref = tupleIndex(dst, tuple_t, 1);
        CHECK(store(c, data_ref, cast(nkl_get_ptr(dst_type->as.slice.target_type), src)));
        CHECK(store(c, size_ref, makeValue<u64>(c, u64_t, src_type->as.ptr.target_type->as.arr.elem_count)));
        return makeRef(dst);
    }
    if (nklt_tclass(dst_type) == NklType_Any && nklt_tclass(src_type) == NkType_Ptr) {
        auto const struct_t = dst_type->underlying_type;
        auto const tuple_t = struct_t->underlying_type;
        auto data_ref = tupleIndex(dst, tuple_t, 0);
        auto type_ref = tupleIndex(dst, tuple_t, 1);
        CHECK(store(c, data_ref, cast(void_ptr_t, src)));
        CHECK(store(c, type_ref, makeValue<nkltype_t>(c, typeref_t, src_type->as.ptr.target_type)));
        return makeRef(dst);
    }
    if (nklt_tclass(dst_type) == NklType_Union) {
        for (usize i = 0; i < dst_type->as.strct.fields.size; i++) {
            if (nklt_typeid(dst_type->as.strct.fields.data[i].type) == nklt_typeid(src_type)) {
                // TODO Implement cast for refs
                auto dst_ref = dst;
                dst_ref.type = tovmt(src_type);
                return store(c, dst_ref, src);
            }
        }
        nk_assert(!"TODO ERROR"); // TODO Report error on failed store to union
    }
    if (nklt_typeid(dst_type) != nklt_typeid(src_type) &&
        !(nklt_tclass(dst_type) == NkType_Ptr && nklt_tclass(src_type) == NkType_Ptr &&
          nklt_tclass(src_type->as.ptr.target_type) == NkType_Array &&
          nklt_typeid(src_type->as.ptr.target_type->as.arr.elem_type) == nklt_typeid(dst_type->as.ptr.target_type))) {
        NkStringBuilder src_sb{};
        NkStringBuilder dst_sb{};
        defer {
            nksb_free(&src_sb);
            nksb_free(&dst_sb);
        };
        nklt_inspect(src_type, &src_sb);
        nklt_inspect(dst_type, &dst_sb);
        return error(
                   c,
                   "cannot store value of type `" NKS_FMT "` into a slot of type `" NKS_FMT "`",
                   NKS_ARG(src_sb),
                   NKS_ARG(dst_sb)),
               ValueInfo{};
    }
    if (nklt_sizeof(src_type)) {
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
            nk_assert(!"unreachable");
            return {};
    }
}

auto pushFn(NklCompiler c, NkIrFunct fn) {
    auto prev_fn = c->cur_fn;
    c->cur_fn = fn;
    return nk_defer([=]() {
        c->cur_fn = prev_fn;
        nkir_activateFunct(c->ir, c->cur_fn);
    });
}

auto pushNode(NklCompiler c, NklAstNode node) {
    if (node && node->id) {
        c->node_stack.emplace_back(NodeInfo{.node = node});
    }
    return nk_defer([=]() {
        if (node && node->id) {
            c->node_stack.pop_back();
        }
    });
}

ComptimeConst comptimeCompileNode(NklCompiler c, NklAstNode node, nkltype_t type = nullptr);
nklval_t comptimeCompileNodeGetValue(NklCompiler c, NklAstNode node, nkltype_t type = nullptr);
ValueInfo compile(NklCompiler c, NklAstNode node, nkltype_t type = nullptr, TagInfoArray tags = {});
Void compileStmt(NklCompiler c, NklAstNode node, nkltype_t type = nullptr, TagInfoArray tags = {});

ValueInfo getStructIndex(NklCompiler c, ValueInfo const &lhs, nkltype_t struct_t, NkAtom name) {
    auto index = nklt_struct_index(struct_t, name);
    if (index == -1ull) {
        NkStringBuilder sb{};
        defer {
            nksb_free(&sb);
        };
        nklt_inspect(lhs.type, &sb);
        return error(c, "no field named `%s` in type `" NKS_FMT "`", nk_atom2cs(name), NKS_ARG(sb)), ValueInfo{};
    }
    return makeRef(tupleIndex(asRef(c, lhs), struct_t->underlying_type, index));
}

ValueInfo getUnionIndex(NklCompiler c, ValueInfo const &lhs, nkltype_t struct_t, NkAtom name) {
    // TODO  Boilerplate between getStructIndex and getUnionIndex
    auto index = nklt_struct_index(struct_t, name);
    if (index == -1ull) {
        NkStringBuilder sb{};
        defer {
            nksb_free(&sb);
        };
        nklt_inspect(lhs.type, &sb);
        return error(c, "no field named `%s` in type `" NKS_FMT "`", nk_atom2cs(name), NKS_ARG(sb)), ValueInfo{};
    }
    return cast(struct_t->as.strct.fields.data[index].type, lhs);
}

ValueInfo deref(NklCompiler c, NkIrRef ref) {
    nk_assert(nklt_tclass(fromvmt(ref.type)) == NkType_Ptr);
    auto const type = ref.type->as.ptr.target_type;
    if (ref.is_indirect) {
        DEFINE(val, store(c, nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, ref.type)), makeRef(ref)));
        ref = asRef(c, val);
    }
    ref.is_indirect = true;
    ref.type = type;
    return makeRef(ref);
}

ValueInfo getMember(NklCompiler c, ValueInfo const &lhs, NkAtom name) {
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
                            return error(c, "member `%s` not found", nk_atom2cs(name)), ValueInfo{};
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
            nk_assert(!"unreachable");
            return {};
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
            NkStringBuilder sb{};
            defer {
                nksb_free(&sb);
            };
            nklt_inspect(lhs.type, &sb);
            return error(c, "type `" NKS_FMT "` is not subscriptable", NKS_ARG(sb)), ValueInfo{};
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
                {}, asRef(c, index), asRef(c, makeValue<u64>(c, u64_t, nklt_sizeof(lhs.type->as.arr.elem_type))));
            auto const offset_ref = asRef(c, makeInstr(mul, u64_t));
            auto const add = nkir_make_add({}, data_ref, offset_ref);
            return deref(c, asRef(c, makeInstr(add, elem_ptr_t)));
        }

        case NkType_Tuple: {
            if (!isKnown(index)) {
                return error(c, "comptime value expected in tuple index"), ValueInfo{};
            }
            auto const i = nklval_as(u64, asValue(c, index));
            if (i >= lhs.type->as.tuple.elems.size) {
                return error(c, "tuple index out of range"), ValueInfo{};
            }
            return makeRef(tupleIndex(asRef(c, lhs), lhs.type, i));
        }

        case NklType_Slice: {
            // TODO Using u8 to correctly index slice in c
            auto const elem_t = lhs.type->as.slice.target_type;
            auto const elem_ptr_t = nkl_get_ptr(lhs.type->as.slice.target_type);
            auto data_ref = asRef(c, lhs);
            data_ref.type = tovmt(u8_ptr_t);
            auto const mul =
                nkir_make_mul({}, asRef(c, index), asRef(c, makeValue<u64>(c, u64_t, nklt_sizeof(elem_t))));
            auto const offset_ref = asRef(c, makeInstr(mul, u64_t));
            auto const add = nkir_make_add({}, data_ref, offset_ref);
            return deref(c, asRef(c, makeInstr(add, elem_ptr_t)));
        }

        case NkType_Ptr: {
            return getIndex(c, deref(c, asRef(c, lhs)), index);
        }

        default: {
            NkStringBuilder sb{};
            defer {
                nksb_free(&sb);
            };
            nklt_inspect(lhs.type, &sb);
            return error(c, "type `" NKS_FMT "` is not indexable", NKS_ARG(sb)), ValueInfo{};
        }
    }
}

ValueInfo getLvalueRef(NklCompiler c, NklAstNode node) {
    if (node->id == n_id) {
        auto const name = node->token->text;
        auto const res = resolve(c, nk_s2atom(name));
        switch (res.kind) {
            case Decl_Local:
                return makeRef(nkir_makeFrameRef(c->ir, res.as.local.id));
            case Decl_Global:
                return makeRef(nkir_makeGlobalRef(c->ir, res.as.global.id));
            case Decl_Undefined:
                return error(c, "`" NKS_FMT "` is not defined", NKS_ARG(name)), ValueInfo{};
            case Decl_ExtSym:
            case Decl_Arg:
                return error(c, "cannot assign to `" NKS_FMT "`", NKS_ARG(name)), ValueInfo{};
            default:
                NK_LOG_ERR("unknown decl type");
                nk_assert(!"unreachable");
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
        auto const name = nk_s2atom(narg1(node)->token->text);
        return getMember(c, lhs, name);
        nk_assert(!"unreachable");
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
    auto name_str = nk_s2stdStr(names.data[0].token->text);
    c->comptime_const_names.push(name_str);
    defer {
        c->comptime_const_names.pop();
    };
    NkAtom name = nk_s2atom({name_str.data(), name_str.size()});
    DEFINE(cnst, compile_cnst());
    CHECK(defineComptimeConst(c, name, cnst));
    return makeVoid();
}

NkIrFunct nkl_compileFile(NklCompiler c, fs::path path, bool create_scope = true);

ValueInfo compileFn(NklCompiler c, NklAstNode node, bool is_variadic, NkCallConv call_conv = NkCallConv_Nk) {
    // TODO Refactor fn compilation

    std::vector<NkAtom> params_names;
    std::vector<nkltype_t> params_types;

    auto params = nargs0(node);

    for (usize i = 0; i < params.size; i++) {
        auto const param = &params.data[i];
        auto const name_node = narg0(param);
        auto const type_node = narg1(param);
        params_names.emplace_back(nk_s2atom(name_node->token->text));
        auto const pop_node = pushNode(c, type_node);
        DEFINE(val, comptimeCompileNodeGetValue(c, type_node));
        if (nklt_tclass(val.type) != NklType_Typeref) {
            return error(c, "type expected"), ValueInfo{};
        }
        params_types.emplace_back(nklval_as(nkltype_t, val));
    }

    nkltype_t ret_t;
    if (narg1(node)) {
        DEFINE(val, comptimeCompileNodeGetValue(c, narg1(node)));
        if (nklt_tclass(val.type) != NklType_Typeref) {
            return error(c, "type expected"), ValueInfo{};
        }
        ret_t = nklval_as(nkltype_t, val);
    } else {
        ret_t = void_t;
    }
    nkltype_t args_t = nkl_get_tuple(c->alloc, {params_types.data(), params_types.size()}, sizeof(nkltype_t));

    auto fn_t = nkl_get_fn({ret_t, args_t, call_conv, is_variadic});

    auto fn = nkir_makeFunct(c->ir);
    auto pop_fn = pushFn(c, fn);

    std::string fn_name;
    if (c->node_stack.size() >= 2 && (*(c->node_stack.rbegin() + 1)).node->id == n_comptime_const_def) {
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

    for (usize i = 0; i < params.size; i++) {
        CHECK(defineArg(c, params_names[i], i, params_types[i]));
    }

    CHECK(compileStmt(c, narg2(node)));
    gen(c, nkir_make_ret());

#ifdef ENABLE_LOGGING
    {
        NkStringBuilder sb{};
        defer {
            nksb_free(&sb);
        };
        nkir_inspectFunct(fn, &sb);
        NK_LOG_INF("ir:\n" NKS_FMT, NKS_ARG(sb));
    }
#endif // ENABLE_LOGGING

    if (call_conv == NkCallConv_Nk) {
        return makeValue<void *>(c, fn_t, (void *)fn);
    } else if (call_conv == NkCallConv_Cdecl) {
        auto cl = nkir_makeNativeClosure(c->ir, fn);
        return makeValue<void *>(c, fn_t, *(void **)cl);
    } else {
        nk_assert(!"invalid calling convention");
        return {};
    }
}

ValueInfo compileFnType(NklCompiler c, NklAstNode node, bool is_variadic, NkCallConv call_conv = NkCallConv_Nk) {
    std::vector<NkAtom> params_names;
    std::vector<nkltype_t> params_types;

    // TODO Boilerplate between compileFn and compileFnType

    auto params = nargs0(node);

    for (usize i = 0; i < params.size; i++) {
        auto const param = &params.data[i];
        auto const name_node = narg0(param);
        auto const type_node = narg1(param);
        params_names.emplace_back(nk_s2atom(name_node->token->text));
        auto const pop_node = pushNode(c, type_node);
        DEFINE(val, comptimeCompileNodeGetValue(c, type_node));
        if (nklt_tclass(val.type) != NklType_Typeref) {
            return error(c, "type expected"), ValueInfo{};
        }
        params_types.emplace_back(nklval_as(nkltype_t, val));
    }

    DEFINE(ret_t_val, comptimeCompileNodeGetValue(c, narg1(node)));
    if (nklt_tclass(ret_t_val.type) != NklType_Typeref) {
        return error(c, "type expected"), ValueInfo{};
    }
    auto ret_t = nklval_as(nkltype_t, ret_t_val);
    nkltype_t args_t = nkl_get_tuple(c->alloc, {params_types.data(), params_types.size()}, sizeof(nkltype_t));

    auto fn_t = nkl_get_fn({ret_t, args_t, call_conv, is_variadic});

    return makeValue<nkltype_t>(c, typeref_t, fn_t);
}

Void initFromAst(NklCompiler c, nklval_t val, NklAstNodeArray init_nodes) {
    switch (nklval_tclass(val)) {
        case NkType_Array:
            if (init_nodes.size > nklval_tuple_size(val)) {
                return error(c, "too many values to init array"), Void{};
            }
            for (usize i = 0; i < nklval_array_size(val); i++) {
                CHECK(initFromAst(c, nklval_array_at(val, i), {&init_nodes.data[i], 1}));
            }
            break;
        case NkType_Numeric: {
            if (init_nodes.size > 1) {
                return error(c, "too many values to init numeric"), Void{};
            }
            auto const &node = init_nodes.data[0];
            if (node.id != n_int && node.id != n_float) {
                return error(c, "invalid value to init numeric"), Void{};
            }
            auto str = nk_s2stdStr(node.token->text);
            switch (nklval_typeof(val)->as.num.value_type) {
                case Int8:
                    nklval_as(i8, val) = std::stoll(str);
                    break;
                case Int16:
                    nklval_as(i16, val) = std::stoll(str);
                    break;
                case Int32:
                    nklval_as(i32, val) = std::stoll(str);
                    break;
                case Int64:
                    nklval_as(i64, val) = std::stoll(str);
                    break;
                case Uint8:
                    nklval_as(u8, val) = std::stoull(str);
                    break;
                case Uint16:
                    nklval_as(u16, val) = std::stoull(str);
                    break;
                case Uint32:
                    nklval_as(u32, val) = std::stoull(str);
                    break;
                case Uint64:
                    nklval_as(u64, val) = std::stoull(str);
                    break;
                case Float32:
                    nklval_as(f32, val) = std::stof(str);
                    break;
                case Float64:
                    nklval_as(f64, val) = std::stod(str);
                    break;
            }
            break;
        }
        case NklType_Struct:
        case NkType_Tuple:
            if (init_nodes.size > nklval_tuple_size(val)) {
                return error(c, "too many values to init tuple/struct"), Void{};
            }
            for (usize i = 0; i < nklval_tuple_size(val); i++) {
                CHECK(initFromAst(c, nklval_tuple_at(val, i), {&init_nodes.data[i], 1}));
            }
            break;
        default:
            NK_LOG_ERR("initFromAst is not implemented for this type");
            nk_assert(!"unreachable");
            break;
    }
    return {};
}

ValueInfo compileAndDiscard(NklCompiler c, NklAstNode node) {
    // TODO Boilerplate between comptimeCompileNode and compileAndDiscard
    auto fn = nkir_makeFunct(c->ir);
    auto pop_fn = pushFn(c, fn);
    nkir_startFunct(
        fn,
        nk_cs2s("#comptime"),
        tovmt(nkl_get_fn(NkltFnInfo{void_t, nkl_get_tuple(c->alloc, {nullptr, 0}, 0), NkCallConv_Nk, false})));
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
    std::unordered_map<NkAtom, ValueInfo> *out_inits = nullptr) {
    std::vector<NklField> fields{};

    auto nodes = nargs0(node);
    for (usize i = 0; i < nodes.size; i++) {
        auto const field_node = &nodes.data[i];
        auto const name_node = narg0(field_node);
        auto const type_node = narg1(field_node);
        auto const init_node = narg2(field_node);
        NkAtom const name = nk_s2atom(name_node->token->text);
        nkltype_t type = void_t;
        if (type_node && type_node->id) {
            auto const pop_node = pushNode(c, type_node);
            DEFINE(type_val, comptimeCompileNodeGetValue(c, type_node));
            if (nklval_tclass(type_val) != NklType_Typeref) {
                return error(c, "type expected in composite type"), ValueInfo{};
            }
            type = nklval_as(nkltype_t, type_val);
        }
        fields.emplace_back(
            NklField{
                .name = name,
                .type = type,
            });
        if (out_inits && init_node && init_node->id) {
            auto const pop_node = pushNode(c, init_node);
            DEFINE(init_val, comptimeCompileNodeGetValue(c, init_node, type));
            (*out_inits)[name] = makeValue(c, init_val);
        }
    }

    return makeValue<nkltype_t>(c, typeref_t, create_type({fields.data(), fields.size()}));
}

ValueInfo import(NklCompiler c, fs::path filepath) {
    if (!fs_fileExists(filepath)) {
        auto const str = filepath.string();
        return error(c, "imported file `%.*s` doesn't exist", (int)str.size(), str.c_str()), ValueInfo{};
    }
    filepath = fs_pathCanonical(filepath);
    auto it = c->imports.find(filepath);
    if (it == c->imports.end()) {
        DEFINE(fn, nkl_compileFile(c, filepath));
        std::tie(it, std::ignore) = c->imports.emplace(filepath, fn);
    }
    auto fn = it->second;
    auto fn_val_info = makeValue<decltype(fn)>(c, fromvmt(nkir_functGetType(fn)), fn);
    auto fn_val = asValue(c, fn_val_info);
    auto stem = filepath.stem().string();
    CHECK(defineComptimeConst(c, nk_s2atom({stem.c_str(), stem.size()}), makeValueComptimeConst(fn_val)));
    return fn_val_info;
}

template <class T>
ValueInfo makeNumeric(NklCompiler c, nkltype_t type, char const *str, char const *fmt) {
    T value = 0;
    // TODO Replace sscanf in compiler
    int res = std::sscanf(str, fmt, &value);
    (void)res;
    nk_assert(res > 0 && res != EOF && "numeric constant parsing failed");
    return makeValue<T>(c, type, value);
}

ValueInfo compileStructLiteral(NklCompiler c, nkltype_t struct_t, NklAstNodeArray init_nodes) {
    auto const value_count = init_nodes.size;
    std::vector<NkAtom> names;
    std::vector<ValueInfo> values;
    names.reserve(value_count);
    values.reserve(value_count);
    bool all_known = true;
    for (usize i = 0; i < value_count; i++) {
        auto const init_node = &init_nodes.data[i];
        auto const name_node = narg0(init_node);
        auto const pop_node = pushNode(c, name_node);
        auto const name = (name_node && name_node->id) ? nk_s2atom(name_node->token->text) : 0;
        if (name && std::find(std::begin(names), std::end(names), name) != std::end(names)) {
            return error(c, "duplicate names"), ValueInfo{};
        }
        if (name && nklt_struct_index(struct_t, name) == -1lu) {
            NkStringBuilder sb{};
            defer {
                nksb_free(&sb);
            };
            nklt_inspect(struct_t, &sb);
            return error(c, "no field named `%s` in type `" NKS_FMT "`", nk_atom2cs(name), NKS_ARG(sb)), ValueInfo{};
        }
        names.emplace_back(name);
        APPEND(values, compile(c, narg1(init_node), struct_t->underlying_type->as.tuple.elems.data[i].type));
        if (!isKnown(values.back())) {
            all_known = false;
        }
    }
    NkIrRef ref{};
    nklval_t val{};
    ValueInfo val_info{};
    if (all_known) {
        val_info = makeValue(c, struct_t);
        ASSIGN(val, asValue(c, val_info));
    } else {
        ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(struct_t)));
    }
    auto const fields = struct_t->as.strct.fields;
    std::vector<bool> initted;
    initted.resize(fields.size);
    usize last_positional = 0;
    for (usize vi = 0; vi < value_count; vi++) {
        auto const init_node = &init_nodes.data[vi];
        auto const name_node = narg0(init_node);
        auto const pop_node = pushNode(c, name_node);
        if (vi >= fields.size) {
            return error(c, "too many values"), ValueInfo{};
        }
        auto fi = names[vi] ? nklt_struct_index(struct_t, names[vi]) : vi;
        if (names[vi]) {
            if (fi < last_positional) {
                return error(c, "named argument conflicts with positional"), ValueInfo{};
            }
        } else {
            last_positional++;
        }
        if (all_known) {
            comptimeStore(c, nklval_tuple_at(val, fi), asValue(c, values[vi]));
        } else {
            CHECK(store(c, tupleIndex(ref, struct_t->underlying_type, fi), values[vi]));
        }
        initted.at(fi) = true;
    }
    auto const it = c->struct_inits.find(nklt_typeid(struct_t));
    if (it != c->struct_inits.end()) {
        auto const &inits = it->second;
        for (usize fi = 0; fi < fields.size; fi++) {
            if (!initted[fi]) {
                auto it = inits.find(fields.data[fi].name);
                if (it != inits.end()) {
                    if (all_known) {
                        comptimeStore(c, nklval_tuple_at(val, fi), asValue(c, it->second));
                    } else {
                        CHECK(store(c, tupleIndex(ref, struct_t->underlying_type, fi), it->second));
                    }
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
    if (all_known) {
        return val_info;
    } else {
        return makeRef(ref);
    }
}

ValueInfo compileTupleLiteral(NklCompiler c, nkltype_t type, nkltype_t tuple_t, NklAstNodeArray init_nodes) {
    auto const value_count = init_nodes.size;
    std::vector<NkAtom> names;
    std::vector<ValueInfo> values;
    names.reserve(value_count);
    values.reserve(value_count);
    // TODO bool all_known = true;
    for (usize i = 0; i < value_count; i++) {
        auto const init_node = &init_nodes.data[i];
        auto const name_node = narg0(init_node);
        auto const val_node = narg1(init_node);
        names.emplace_back((name_node && name_node->id) ? nk_s2atom(name_node->token->text) : 0);
        auto const pop_node = pushNode(c, val_node);
        APPEND(values, compile(c, val_node, type->as.tuple.elems.data[i].type));
        // TODO if (!isKnown(values.back())) {
        //     all_known = false;
        // }
    }
    auto const ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(type)));
    if (value_count != tuple_t->as.tuple.elems.size) {
        return error(c, "invalid number of values in composite literal"), ValueInfo{};
    }
    for (usize i = 0; i < value_count; i++) {
        CHECK(store(c, tupleIndex(ref, tuple_t, i), values[i]));
    }
    return makeRef(ref);
}

ValueInfo compile(NklCompiler c, NklAstNode node, nkltype_t type, TagInfoArray tags) {
    NK_PROF_FUNC();
    NK_LOG_DBG("node: %s", s_nkl_ast_node_names[node->id]);

    c->node_stack.emplace_back(
        NodeInfo{
            .node = node,
            .type = type,
            .tags = tags,
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

        case n_null: {
            return makeValue<void *>(
                c,
                (c->node_stack.back().type && nklt_tclass(c->node_stack.back().type) == NkType_Ptr)
                    ? c->node_stack.back().type
                    : void_t,
                nullptr);
        }

#define X(TYPE, VALUE_TYPE)                                          \
    case NK_CAT(n_, TYPE): {                                         \
        return makeValue<nkltype_t>(c, typeref_t, NK_CAT(TYPE, _t)); \
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
            auto const pop_node = pushNode(c, narg0(node));
            DEFINE(val, comptimeCompileNodeGetValue(c, narg0(node)));
            if (nklt_tclass(val.type) != NklType_Typeref) {
                return error(c, "type expected"), ValueInfo{};
            }
            auto target_type = nklval_as(nkltype_t, val);
            return makeValue<nkltype_t>(c, typeref_t, nkl_get_ptr(target_type));
        }

        case n_const_ptr_type: {
            // TODO Ignoring const in const_ptr_type
            auto const pop_node = pushNode(c, narg0(node));
            DEFINE(val, comptimeCompileNodeGetValue(c, narg0(node)));
            if (nklt_tclass(val.type) != NklType_Typeref) {
                return error(c, "type expected"), ValueInfo{};
            }
            auto target_type = nklval_as(nkltype_t, val);
            return makeValue<nkltype_t>(c, typeref_t, nkl_get_ptr(target_type));
        }

        case n_slice_type: {
            auto const pop_node = pushNode(c, narg0(node));
            DEFINE(val, comptimeCompileNodeGetValue(c, narg0(node)));
            if (nklt_tclass(val.type) != NklType_Typeref) {
                return error(c, "type expected"), ValueInfo{};
            }
            auto target_type = nklval_as(nkltype_t, val);
            auto type = nkl_get_slice(c->alloc, target_type);
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

#define COMPILE_BIN(NAME)                                                                       \
    case NK_CAT(n_, NAME): {                                                                    \
        DEFINE(lhs, compile(c, narg0(node), c->node_stack.back().type));                        \
        DEFINE(rhs, compile(c, narg1(node), lhs.type));                                         \
        return makeInstr(NK_CAT(nkir_make_, NAME)({}, asRef(c, lhs), asRef(c, rhs)), lhs.type); \
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

#define COMPILE_BOOL_BIN(NAME)                                                                \
    case NK_CAT(n_, NAME): {                                                                  \
        DEFINE(lhs, compile(c, narg0(node)));                                                 \
        DEFINE(rhs, compile(c, narg1(node), lhs.type));                                       \
        return makeInstr(NK_CAT(nkir_make_, NAME)({}, asRef(c, lhs), asRef(c, rhs)), bool_t); \
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
            auto const pop_node = pushNode(c, narg0(node));
            DEFINE(type_val, comptimeCompileNodeGetValue(c, narg0(node)));
            if (nklt_tclass(type_val.type) != NklType_Typeref) {
                return error(c, "type expected"), ValueInfo{};
            }
            auto type = nklval_as(nkltype_t, type_val);
            auto const pop_node2 = pushNode(c, narg1(node));
            DEFINE(size_val, comptimeCompileNodeGetValue(c, narg1(node), u64_t));
            if (nklt_tclass(size_val.type) != NkType_Numeric || size_val.type->as.num.value_type != Uint64) {
                return error(c, "u64 expected"), ValueInfo{};
            }
            auto size = nklval_as(u64, size_val);
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
                                return error(c, "cannot cast pointer to any numeric type other than u64"), ValueInfo{};
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
            }

            NkStringBuilder src_sb{};
            NkStringBuilder dst_sb{};
            defer {
                nksb_free(&src_sb);
                nksb_free(&dst_sb);
            };
            nklt_inspect(src_type, &src_sb);
            nklt_inspect(dst_type, &dst_sb);
            return error(
                       c,
                       "cannot cast value of type `" NKS_FMT "` to type `" NKS_FMT "`",
                       NKS_ARG(src_sb),
                       NKS_ARG(dst_sb)),
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
            for (usize i = 0; i < nodes.size; i++) {
                CHECK(compileStmt(c, &nodes.data[i]));
            }
            return makeVoid();
        }

        case n_tuple: {
            auto nodes = nargs0(node);
            std::vector<nkltype_t> types;
            std::vector<ValueInfo> values;
            values.reserve(nodes.size);
            for (usize i = 0; i < nodes.size; i++) {
                APPEND(values, compile(c, &nodes.data[i]));
                types.emplace_back(values.back().type);
            }
            auto tuple_t = nkl_get_tuple(c->alloc, {types.data(), types.size()}, sizeof(nkltype_t));
            auto const ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(tuple_t)));
            if (values.size() != tuple_t->as.tuple.elems.size) {
                return error(c, "invalid number of values in tuple literal"), ValueInfo{};
            }
            for (usize i = 0; i < values.size(); i++) {
                CHECK(store(c, tupleIndex(ref, tuple_t, i), values[i]));
            }
            return makeRef(ref);
        }

        case n_tuple_type: {
            auto nodes = nargs0(node);
            std::vector<nkltype_t> types;
            types.reserve(nodes.size);
            for (usize i = 0; i < nodes.size; i++) {
                DEFINE(val, comptimeCompileNodeGetValue(c, &nodes.data[i]));
                if (nklval_tclass(val) != NklType_Typeref) {
                    return error(c, "type expected in tuple type"), ValueInfo{};
                }
                types.emplace_back(nklval_as(nkltype_t, val));
            }
            return makeValue<nkltype_t>(
                c, typeref_t, nkl_get_tuple(c->alloc, {types.data(), types.size()}, sizeof(nkltype_t)));
        }

        case n_import: {
            auto const name = narg0(node)->token->text;
            std::string const filename = nk_s2stdStr(name) + ".nkl";
            auto corelib_path = fs::path{c->corelib_dir};
            if (!corelib_path.is_absolute()) {
                corelib_path = fs::path{c->compiler_dir} / corelib_path;
            }
            auto const filepath = (corelib_path / filename).lexically_normal();
            return import(c, filepath);
        }

        case n_import_path: {
            NkString const text{node->token->text.data + 1, node->token->text.size - 2};
            auto const name = text;
            auto filepath = fs::path(nk_s2stdView(name)).lexically_normal();
            if (!filepath.is_absolute()) {
                filepath = (c->file_stack.top().parent_path() / filepath).lexically_normal();
            }
            return import(c, filepath);
        }

        case n_id: {
            NkString name_str = node->token->text;
            NkAtom name = nk_s2atom(name_str);
            auto &decl = resolve(c, name);
            if (decl.kind == Decl_Undefined) {
                return error(c, "`" NKS_FMT "` is not defined", NKS_ARG(name_str)), ValueInfo{};
            } else if (
                (decl.kind == Decl_Arg && decl.as.arg.fn != curScope(c).cur_fn) ||
                (decl.kind == Decl_Local && decl.as.local.fn != curScope(c).cur_fn)) {
                return error(c, "cannot reference `" NKS_FMT "` through procedure frame boundary", NKS_ARG(name_str)),
                       ValueInfo{};
            } else {
                return declToValueInfo(decl);
            }
        }

        case n_intrinsic: {
            NkString name_str = node->token->text;
            NkAtom name = nk_s2atom(name_str);
            if (name == nk_cs2atom("@typeof")) {
                return error(c, "invalid use of `" NKS_FMT "` intrinsic", NKS_ARG(name_str)), ValueInfo{};
            } else {
                return error(c, "invalid intrinsic `" NKS_FMT "`", NKS_ARG(name_str)), ValueInfo{};
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
                    return makeNumeric<f32>(c, f32_t, text.data, "%" SCNf32);
                default:
                case Float64:
                    return makeNumeric<f64>(c, f64_t, text.data, "%" SCNf64);
            }
        }

        case n_int: {
            NkNumericValueType value_type =
                (c->node_stack.back().type && nklt_tclass(c->node_stack.back().type) == NkType_Numeric)
                    ? c->node_stack.back().type->as.num.value_type
                    : Int64;
            auto const text = node->token->text;
            switch (value_type) {
#define X(TYPE, VALUE_TYPE) \
    case VALUE_TYPE:        \
        return makeNumeric<TYPE>(c, NK_CAT(TYPE, _t), text.data, "%" NK_CAT(SCN, TYPE));
                NUMERIC_ITERATE(X)
#undef X
                default:
                    nk_assert(!"unreachable");
                    return {};
            }
            nk_assert(!"unreachable");
            return {};
        }

        case n_int_hex: {
            NkNumericValueType value_type =
                (c->node_stack.back().type && nklt_tclass(c->node_stack.back().type) == NkType_Numeric)
                    ? c->node_stack.back().type->as.num.value_type
                    : Int64;
            auto const text = node->token->text;
            switch (value_type) {
#define X(TYPE, VALUE_TYPE) \
    case VALUE_TYPE:        \
        return makeNumeric<TYPE>(c, NK_CAT(TYPE, _t), text.data + 2, "%" NK_CAT(SCNX, TYPE));
                NUMERIC_ITERATE_INT(X)
#undef X
                default:
                    nk_assert(!"unreachable");
                    return {};
            }
            nk_assert(!"unreachable");
            return {};
        }

        case n_string: {
            NkString const text{node->token->text.data + 1, node->token->text.size - 2};

            auto ar_t = nkl_get_array(i8_t, text.size + 1);
            auto str_t = nkl_get_ptr(ar_t);

            auto str = nk_arena_allocT<char>(&c->arena, text.size + 1);
            std::memcpy(str, text.data, text.size);
            str[text.size] = '\0';

            return makeValue<void *>(c, str_t, (void *)str);
        }

        case n_escaped_string: {
            NkString const text{node->token->text.data + 1, node->token->text.size - 2};

            NkStringBuilder sb{};
            defer {
                nksb_free(&sb);
            };
            nks_unescape(nksb_getStream(&sb), text);
            nksb_appendNull(&sb);

            auto ar_t = nkl_get_array(i8_t, sb.size);
            auto str_t = nkl_get_ptr(ar_t);

            auto str = nk_arena_allocT<char>(&c->arena, sb.size);
            std::memcpy(str, sb.data, sb.size);

            return makeValue<void *>(c, str_t, (void *)str);
        }

        case n_member: {
            return getLvalueRef(c, node);
        }

        case n_struct: {
            std::unordered_map<NkAtom, ValueInfo> inits;
            DEFINE(
                type_val,
                compileCompositeType(
                    c,
                    node,
                    [c](NklFieldArray fields) {
                        return nkl_get_struct(c->alloc, fields);
                    },
                    &inits));
            if (!inits.empty()) {
                c->struct_inits[nklt_typeid(nklval_as(nkltype_t, asValue(c, type_val)))] = std::move(inits);
            }
            return type_val;
        }

        case n_union: {
            return compileCompositeType(c, node, [c](NklFieldArray fields) {
                return nkl_get_union(c->alloc, fields);
            });
        }

        case n_enum: {
            return compileCompositeType(c, node, [c](NklFieldArray fields) {
                return nkl_get_enum(c->alloc, fields);
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
            auto const n_name = narg0(node);
            auto const n_args = nargs1(node);
            auto const n_nodes = nargs2(node);

            auto const name = nk_s2atom(n_name->token->text);

            auto &decl = resolve(c, name);
            if (decl.kind == Decl_Undefined) {
                return error(c, "undefined tag"), ValueInfo{};
            }
            nk_assert(decl.kind == Decl_ComptimeConst && "tag must be a comptime const");

            auto const type_val = comptimeConstGetValue(c, decl.as.comptime_const);
            nk_assert(nklt_tclass(type_val.type) == NklType_Typeref && "tag must be a type");

            auto const type = nklval_as(nkltype_t, type_val);
            if (nklt_tclass(type) != NklType_Struct) {
                return error(c, "TODO For now tag only can be a struct"), ValueInfo{};
            }

            auto val = compileStructLiteral(c, type, n_args);
            if (!isKnown(val)) {
                return error(c, "comptime constant expected in tag"), ValueInfo{};
            }

            TagInfo const tag{
                .name = name,
                .val{asValue(c, val)},
            };

            // TODO Support multiple tags
            TagInfoArray tags{&tag, 1lu};

            if (n_nodes.size == 1) {
                return compile(c, &n_nodes.data[0], nullptr, tags);
            } else {
                for (usize i = 0; i < n_nodes.size; i++) {
                    CHECK(compileStmt(c, &n_nodes.data[i], nullptr, tags));
                }
                return makeVoid();
            }
        }

        case n_call: {
            if (narg0(node)->id == n_intrinsic) {
                NkAtom name = nk_s2atom(narg0(node)->token->text);
                if (name == nk_cs2atom("@typeof")) {
                    auto arg_nodes = nargs1(node);
                    if (arg_nodes.size != 1) {
                        return error(c, "@typeof expects exactly one argument"), ValueInfo{};
                    }
                    DEFINE(val, compileAndDiscard(c, narg1(&arg_nodes.data[0])));
                    return makeValue<nkltype_t>(c, typeref_t, val.type);
                } else if (name == nk_cs2atom("@sizeof")) {
                    auto arg_nodes = nargs1(node);
                    if (arg_nodes.size != 1) {
                        return error(c, "@sizeof expects exactly one argument"), ValueInfo{};
                    }
                    DEFINE(val, compileAndDiscard(c, narg1(&arg_nodes.data[0])));
                    return makeValue<u64>(c, u64_t, nklt_sizeof(val.type));
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
                               "invalid number of arguments, expected at least `%zu`, provided `%zu`",
                               fn_t->as.fn.args_t->as.tuple.elems.size,
                               arg_nodes.size),
                           ValueInfo{};
                }
            } else {
                if (arg_nodes.size != fn_t->as.fn.args_t->as.tuple.elems.size) {
                    return error(
                               c,
                               "invalid number of arguments, expected `%zu`, provided `%zu`",
                               fn_t->as.fn.args_t->as.tuple.elems.size,
                               arg_nodes.size),
                           ValueInfo{};
                }
            }

            std::vector<ValueInfo> args_info{};
            std::vector<nkltype_t> args_types{};

            args_info.reserve(arg_nodes.size);
            args_types.reserve(arg_nodes.size);

            for (usize i = 0; i < arg_nodes.size; i++) {
                auto param_t = i < fn_t->as.fn.args_t->as.tuple.elems.size
                                   ? fn_t->as.fn.args_t->as.tuple.elems.data[i].type
                                   : nullptr;
                // TODO Support named args in call
                DEFINE(val_info, compile(c, narg1(&arg_nodes.data[i]), param_t));
                args_info.emplace_back(val_info);
                args_types.emplace_back(param_t ? param_t : val_info.type);
            }

            auto args_t = nkl_get_tuple(c->alloc, {args_types.data(), args_types.size()}, sizeof(nkltype_t));

            NkIrRef args_ref{};
            if (nklt_sizeof(args_t)) {
                args_ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(args_t)));
            }

            for (usize i = 0; i < args_info.size(); i++) {
                auto const arg_ref = tupleIndex(args_ref, args_t, i);
                CHECK(store(c, arg_ref, args_info[i]));
            }

            return makeInstr(nkir_make_call({}, asRef(c, lhs), args_ref), fn_t->as.fn.ret_t);
        }

        case n_object_literal: {
            DEFINE(type_val, comptimeCompileNodeGetValue(c, narg0(node)));

            if (nklval_tclass(type_val) != NklType_Typeref) {
                return error(c, "type expected in object literal"), ValueInfo{};
            }

            auto type = nklval_as(nkltype_t, type_val);
            auto const init_nodes = nargs1(node);

            // TODO Support compile time object literals other than struct

            switch (nklt_tclass(type)) {
                case NklType_Struct: {
                    return compileStructLiteral(c, type, init_nodes);
                }

                case NklType_Any:
                case NklType_Slice: {
                    auto const struct_t = type->underlying_type;
                    auto const tuple_t = struct_t->underlying_type;
                    return compileTupleLiteral(c, type, tuple_t, init_nodes);
                }

                case NkType_Tuple: {
                    return compileTupleLiteral(c, type, type, init_nodes);
                }

                case NkType_Array: {
                    auto const value_count = init_nodes.size;
                    if (value_count != type->as.arr.elem_count) {
                        return error(c, "invalid number of values in array literal"), ValueInfo{};
                    }
                    std::vector<NkAtom> names;
                    std::vector<ValueInfo> values;
                    names.reserve(value_count);
                    values.reserve(value_count);
                    // TODO bool all_known = true;
                    for (usize i = 0; i < value_count; i++) {
                        auto const init_node = &init_nodes.data[i];
                        auto const name_node = narg0(init_node);
                        auto const val_node = narg1(init_node);
                        names.emplace_back((name_node && name_node->id) ? nk_s2atom(name_node->token->text) : 0);
                        auto const pop_node = pushNode(c, val_node);
                        APPEND(values, compile(c, val_node, type->as.arr.elem_type));
                        // TODO if (!isKnown(values.back())) {
                        //     all_known = false;
                        // }
                    }
                    auto const ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(type)));
                    for (usize i = 0; i < value_count; i++) {
                        CHECK(store(c, arrayIndex(ref, type, i), values[i]));
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
                    auto const name = nk_s2atom(name_node->token->text);
                    auto const struct_t = type->underlying_type;
                    auto const union_t = struct_t->as.strct.fields.data[0].type;
                    // TODO Can we not create a local variable undonditionally in enum literal?
                    auto const enum_ref = nkir_makeFrameRef(c->ir, nkir_makeLocalVar(c->ir, tovmt(type)));
                    auto const data_ref = tupleIndex(enum_ref, struct_t->underlying_type, 0);
                    auto const tag_ref = tupleIndex(enum_ref, struct_t->underlying_type, 1);
                    DEFINE(field_ref, getUnionIndex(c, makeRef(data_ref), union_t, name));
                    DEFINE(value, compile(c, narg1(init_node), field_ref.type));
                    CHECK(store(c, asRef(c, field_ref), value));
                    // TODO Indexing union twice in enum literal
                    auto const index = nklt_struct_index(union_t, name);
                    CHECK(store(c, tag_ref, makeValue<u64>(c, u64_t, index)));
                    return makeRef(enum_ref);
                }

                default: {
                    nklval_t val{nk_arena_alloc(&c->arena, nklt_sizeof(type)), type};
                    std::memset(nklval_data(val), 0, nklval_sizeof(val));

                    // TODO Ignoring named args in object literal
                    std::vector<NklAstNode_T> nodes;
                    nodes.reserve(init_nodes.size);
                    for (usize i = 0; i < init_nodes.size; i++) {
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
            for (usize i = 0; i < lhs_nodes.size; i++) {
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
                for (usize i = 0; i < lhss.size(); i++) {
                    auto rhs_ref = asRef(c, rhs);
                    store(c, asRef(c, lhss[i]), makeRef(tupleIndex(rhs_ref, rhs.type, i)));
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
            NkAtom name = nk_s2atom(names.data[0].token->text);
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
            std::optional<LinkTag> opt_link_tag{};
            std::optional<ExternTag> opt_extern_tag{};

            for (auto const &tag : nk_iterate(c->node_stack.back().tags)) {
                if (tag.name == nk_cs2atom("#link")) {
                    opt_link_tag = nklval_as(LinkTag, tag.val);
                } else if (tag.name == nk_cs2atom("#extern")) {
                    opt_extern_tag = nklval_as(ExternTag, tag.val);
                }
            }

            auto const def_id = narg1(node)->id;

            if (opt_link_tag && (def_id == n_fn_type || def_id == n_fn_type_var)) {
                auto &link = opt_link_tag.value();

                // TODO Treating slice as cstring, while we include excess zero charater
                auto soname = std::string{nk_s2stdStr(link.libname).c_str()};
                if (soname == "c" || soname == "C") {
                    soname = c->libc_name;
                } else if (soname == "m" || soname == "M") {
                    soname = c->libm_name;
                }

                // TODO Treating slice as cstring, while we include excess zero charater
                auto link_prefix = std::string{nk_s2stdStr(link.prefix).c_str()};

                auto so = nkir_makeShObj(c->ir, nk_cs2s(soname.c_str())); // TODO Creating so every time

                auto sym_name = narg0(node)->token->text;
                auto sym_name_with_prefix_std_str = link_prefix + nk_s2stdStr(sym_name);
                NkString sym_name_with_prefix{sym_name_with_prefix_std_str.data(), sym_name_with_prefix_std_str.size()};

                bool const is_variadic = def_id == n_fn_type_var;
                // TODO #link implies cdecl
                DEFINE(fn_t_info, compileFnType(c, narg1(node), is_variadic, NkCallConv_Cdecl));
                DEFINE(fn_t_val, asValue(c, fn_t_info));
                auto fn_t = nklval_as(nkltype_t, fn_t_val);

                CHECK(defineExtSym(
                    c, nk_s2atom(sym_name), nkir_makeExtSym(c->ir, so, sym_name_with_prefix, tovmt(fn_t)), fn_t));

                return makeVoid();
            } else if (opt_extern_tag) {
                auto const call_conv = NkCallConv_Cdecl; // TODO Ignoring ABI in #extern
                if (def_id == n_fn || def_id == n_fn_var) {
                    return compileComptimeConstDef(c, node, [=]() -> ComptimeConst {
                        bool const is_variadic = def_id == n_fn_var;
                        DEFINE(fn_info, compileFn(c, narg1(node), is_variadic, call_conv));
                        DEFINE(fn_val, asValue(c, fn_info));
                        return makeValueComptimeConst(fn_val);
                    });
                } else if (def_id == n_fn_type || def_id == n_fn_type_var) {
                    return compileComptimeConstDef(c, node, [=]() -> ComptimeConst {
                        bool const is_variadic = def_id == n_fn_type_var;
                        DEFINE(fn_t_info, compileFnType(c, narg1(node), is_variadic, call_conv));
                        DEFINE(fn_t_val, asValue(c, fn_t_info));
                        return makeValueComptimeConst(fn_t_val);
                    });
                }
            }

            return compileComptimeConstDef(c, node, [=]() {
                return comptimeCompileNode(c, narg1(node));
            });
        }

        case n_tag_def: {
            return compileComptimeConstDef(c, node, [=]() {
                auto cnst = comptimeCompileNode(c, narg1(node));
                if (nklt_tclass(comptimeConstType(cnst)) != NklType_Typeref) {
                    return error(c, "type expected in tag definition"), ComptimeConst{};
                }
                return cnst;
            });
        }

        case n_var_decl: {
            auto const &names = nargs0(node);
            if (names.size > 1) {
                return error(c, "TODO multiple assignment is not implemented"), ValueInfo{};
            }
            NkAtom name = nk_s2atom(names.data[0].token->text);
            auto const pop_node = pushNode(c, narg1(node));
            DEFINE(type_val, comptimeCompileNodeGetValue(c, narg1(node)));
            if (nklt_tclass(type_val.type) != NklType_Typeref) {
                return error(c, "type expected"), ValueInfo{};
            }
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
    NK_PROF_FUNC();

    auto fn = nkir_makeFunct(c->ir);
    NkltFnInfo fn_info{nullptr, nkl_get_tuple(c->alloc, {nullptr, 0}, 0), NkCallConv_Nk, false};

    auto pop_fn = pushFn(c, fn);

    auto const vm_fn_info = tovmf(fn_info);
    nkir_startIncompleteFunct(fn, nk_cs2s("#comptime"), &vm_fn_info);
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

#ifdef ENABLE_LOGGING
        {
            NkStringBuilder sb{};
            defer {
                nksb_free(&sb);
            };
            nkir_inspectFunct(fn, &sb);
            NK_LOG_INF("ir:\n" NKS_FMT, NKS_ARG(sb));
        }
#endif // ENABLE_LOGGING
    }

    return cnst;
}

nklval_t comptimeCompileNodeGetValue(NklCompiler c, NklAstNode node, nkltype_t type) {
    NK_PROF_FUNC();

    DEFINE(cnst, comptimeCompileNode(c, node, type));
    // TODO Probably should have a way of controlling, whether we want to
    // automatically invoke code at compile time here
    return comptimeConstGetValue(c, cnst);
}

Void compileStmt(NklCompiler c, NklAstNode node, nkltype_t type, TagInfoArray tags) {
    NK_PROF_FUNC();

    DEFINE(val, compile(c, node, type, tags));
    auto ref = asRef(c, val);
    if (val.kind != v_none) {
        (void)ref;
        // TODO Boilerplate for debug printing
#ifdef ENABLE_LOGGING
        NkStringBuilder sb{};
        defer {
            nksb_free(&sb);
        };
        nkir_inspectRef(c->ir, ref, &sb);
        NK_LOG_DBG("value ignored: " NKS_FMT, NKS_ARG(sb));
#endif // ENABLE_LOGGING
    }
    return {};
}

NkIrFunct nkl_compile(NklCompiler c, NklAstNode root, bool create_scope = true) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    auto fn = nkir_makeFunct(c->ir);
    auto pop_fn = pushFn(c, fn);

    auto top_level_fn_t = nkl_get_fn({void_t, nkl_get_tuple(c->alloc, {nullptr, 0}, 0), NkCallConv_Nk, false});

    nkir_startFunct(fn, nk_cs2s("#top_level"), tovmt(top_level_fn_t));
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

#ifdef ENABLE_LOGGING
    {
        NkStringBuilder sb{};
        defer {
            nksb_free(&sb);
        };
        nkir_inspectFunct(fn, &sb);
        nkir_inspectExtSyms(c->ir, &sb);
        NK_LOG_INF("ir:\n" NKS_FMT, NKS_ARG(sb));
    }
#endif // ENABLE_LOGGING

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
        to_color ? NK_TERM_COLOR_RED : "",
        (int)(token->text.size),
        src.data() + token->pos,
        to_color ? NK_TERM_COLOR_NONE : "",
        (int)(next_newline - token->pos - token->text.size),
        src.data() + token->pos + token->text.size,
        to_color ? NK_TERM_COLOR_RED : "",
        (int)(token->col + std::log10(token->lin) + 4),
        "^");
    if (token->text.size) {
        for (usize i = 0; i < token->text.size - 1; i++) {
            std::fprintf(stderr, "%c", '~');
        }
    }
    std::fprintf(stderr, "%s\n", to_color ? NK_TERM_COLOR_NONE : "");
}

void printError(NklCompiler c, NklTokenRef token, std::string const &err_str) {
    if (c->error_reported) {
        return;
    }

    // TODO Refactor coloring
    // TODO Add option to control coloring from CLI
    bool const to_color = nk_isatty(2);
    nk_assert(!c->file_stack.empty());
    std::fprintf(
        stderr,
        "%s%s:%zu:%zu:%s %serror:%s %.*s\n",
        to_color ? NK_TERM_COLOR_WHITE : "",
        c->file_stack.top().string().c_str(),
        token->lin,
        token->col,
        to_color ? NK_TERM_COLOR_NONE : "",
        to_color ? NK_TERM_COLOR_RED : "",
        to_color ? NK_TERM_COLOR_NONE : "",
        (int)err_str.size(),
        err_str.data());

    nk_assert(!c->src_stack.empty());
    auto src = nk_s2stdView(c->src_stack.top());

    printQuote(src, token, to_color);

    c->error_occurred = true;
    c->error_reported = true;
}

NK_PRINTF_LIKE(2) void printError(NklCompiler c, char const *fmt, ...) {
    if (c->error_reported) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    NkStringBuilder sb{};
    defer {
        nksb_free(&sb);
    };
    nksb_vprintf(&sb, fmt, ap);
    NkString str{NKS_INIT(sb)};
    va_end(ap);

    bool const to_color = nk_isatty(2);

    std::fprintf(
        stderr,
        "%serror:%s " NKS_FMT "\n",
        to_color ? NK_TERM_COLOR_RED : "",
        to_color ? NK_TERM_COLOR_NONE : "",
        NKS_ARG(str));

    c->error_occurred = true;
    c->error_reported = true;
}

NkIrFunct nkl_compileSrc(NklCompiler c, NkString src, bool create_scope = true) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

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
    auto root = nkl_parse(ast, {tokens.data(), tokens.size()}, err_str, err_token);
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
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    if (!fs_fileExists(path)) {
        auto const path_str = path.string();
        printError(c, "file `%.*s` doesn't exist", (int)path_str.size(), path_str.c_str());
        return {};
    }

    path = fs_pathCanonical(path);

    auto const path_str = path.string();

    c->file_stack.emplace(fs_pathRelative(path));
    defer {
        c->file_stack.pop();
    };

    NkString src;
    bool const ok = nk_file_read(c->alloc, {path_str.c_str(), path_str.size()}, &src);
    if (ok) {
        return nkl_compileSrc(c, src, create_scope);
    } else {
        printError(c, "failed to open file `%.*s`", (int)path_str.size(), path_str.c_str());
        return {};
    }
}

template <class T>
T getConfigValue(NklCompiler c, std::string const &name, decltype(Scope::locals) &config) {
    auto it = config.find(nk_cs2atom(name.c_str()));
    ValueInfo val_info{};
    if (it == config.end()) {
        error(c, "`%.*s` is missing in config", (int)name.size(), name.c_str());
    } else {
        val_info = declToValueInfo(it->second);
        if (!isKnown(val_info)) {
            error(c, "`%.*s` value is not known", (int)name.size(), name.c_str());
        }
    }
    if (c->error_occurred) {
        printError(c, NKS_FMT, (int)c->err_str.size(), c->err_str.c_str());
        return T{};
    }
    return nklval_as(T, asValue(c, val_info)); // TODO Reinterpret cast in compiler without check
}

} // namespace

extern "C" NK_EXPORT Void nkl_compiler_declareLocal(NkString name, nkltype_t type) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);
    NklCompiler c = s_compiler;
    // TODO Treating slice as cstring, while we include excess zero charater
    CHECK(defineLocal(c, nk_cs2atom(nk_s2stdStr(name).c_str()), nkir_makeLocalVar(c->ir, tovmt(type)), type));
    return {};
}

struct LinkedLib {
    std::string lib_str;
    bool is_file;
};

struct NklCompilerBuilder {
    std::vector<LinkedLib> libs{};
};

extern "C" NK_EXPORT NklCompilerBuilder *nkl_compiler_createBuilder() {
    NK_LOG_TRC("%s", __func__);

    return new (nk_alloc(nk_default_allocator, sizeof(NklCompilerBuilder))) NklCompilerBuilder{};
}

extern "C" NK_EXPORT void nkl_compiler_freeBuilder(NklCompilerBuilder *b) {
    NK_LOG_TRC("%s", __func__);

    b->~NklCompilerBuilder();
    nk_free(nk_default_allocator, b, sizeof(*b));
}

extern "C" NK_EXPORT bool nkl_compiler_link(NklCompilerBuilder *b, NkString lib) {
    NK_LOG_TRC("%s", __func__);
    b->libs.emplace_back(LinkedLib{nk_s2stdStr(lib), false});
    return true;
}

extern "C" NK_EXPORT bool nkl_compiler_linkFile(NklCompilerBuilder *b, NkString lib) {
    NK_LOG_TRC("%s", __func__);
    b->libs.emplace_back(LinkedLib{nk_s2stdStr(lib), true});
    return true;
}

extern "C" NK_EXPORT bool nkl_compiler_build(NklCompilerBuilder *b, NkIrFunct entry, NkString exe_name) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);

    NklCompiler c = s_compiler;

    std::string flags = c->c_compiler_flags;
    for (auto const &lib : b->libs) {
        fs::path path{lib.lib_str};
        if (fs_fileExists(path)) {
            path = fs_pathCanonical(path);
        }
        if (!path.parent_path().empty()) {
            flags += " -L" + path.parent_path().string();
            flags += " -Wl,-rpath=" + path.parent_path().string();
        }
        flags += (lib.is_file ? " -l:" : " -l") + path.filename().string();
    }

    NkIrCompilerConfig conf{
        .compiler_binary = {c->c_compiler.c_str(), c->c_compiler.size()},
        .additional_flags = {flags.c_str(), flags.size()},
        .output_filename = exe_name,
        .quiet = 0,
    };

    NkArena scratch{};
    bool ret = nkir_compile(&scratch, conf, c->ir, entry);
    nk_arena_free(&scratch);
    return ret;
}

struct StructField {
    NkString name;
    nkltype_t type;
};

typedef NkSlice(StructField) StructFieldArray;

extern "C" NK_EXPORT nkltype_t nkl_compiler_makeStruct(StructFieldArray fields_raw) {
    NklCompiler c = s_compiler;
    std::vector<NklField> fields;
    fields.reserve(fields_raw.size);
    for (auto const &field : nk_iterate(fields_raw)) {
        fields.emplace_back(
            NklField{
                // TODO Treating slice as cstring, while we include excess zero charater
                .name = nk_cs2atom(nk_s2stdStr(field.name).c_str()),
                .type = field.type,
            });
    }
    return nkl_get_struct(c->alloc, {fields.data(), fields.size()});
}

NklCompiler nkl_compiler_create() {
    NK_PROF_FUNC();

    nkl_ast_init();

    NkArena arena{};

#define X(TYPE, VALUE_TYPE) NK_CAT(TYPE, _t) = nkl_get_numeric(VALUE_TYPE);
    NUMERIC_ITERATE(X)
#undef X

    any_t = nkl_get_any(nk_arena_getAllocator(&arena));
    typeref_t = nkl_get_typeref();
    void_t = nkl_get_void();
    void_ptr_t = nkl_get_ptr(void_t);
    bool_t = u8_t; // Modeling bool as u8
    u8_ptr_t = nkl_get_ptr(u8_t);

    auto c = new (nk_alloc(nk_default_allocator, sizeof(NklCompiler_T))) NklCompiler_T{
        .ir = nkir_createProgram(),
        .arena = arena,
    };
    c->alloc = nk_arena_getAllocator(&c->arena);
    return c;
}

void nkl_compiler_free(NklCompiler c) {
    nk_arena_free(&c->arena);
    nkir_deinitProgram(c->ir);
    c->~NklCompiler_T();
    nk_free(nk_default_allocator, c, sizeof(*c));
}

bool nkl_compiler_configure(NklCompiler c, NkString self_path) {
    NK_PROF_FUNC();
    NK_LOG_TRC("%s", __func__);
    NK_LOG_DBG("self_path=`" NKS_FMT "`", NKS_ARG(self_path));

    c->compiler_dir = fs::path(nk_s2stdStr(self_path)).parent_path().string();

    pushScope(c);
    auto preload_filepath = fs::path{c->compiler_dir} / "preload.nkl";
    if (!fs_fileExists(preload_filepath)) {
        auto const path_str = preload_filepath.string();
        printError(c, "preload file `%.*s` doesn't exist", (int)path_str.size(), path_str.c_str());
        return false;
    }
    if (!nkl_compileFile(c, preload_filepath, false)) {
        return false;
    }

    auto config_filepath = fs::path{c->compiler_dir} / "config.nkl";
    if (!fs_fileExists(config_filepath)) {
        auto const path_str = config_filepath.string();
        printError(c, "config file `%.*s` doesn't exist", (int)path_str.size(), path_str.c_str());
        return false;
    }
    auto fn = nkl_compileFile(c, config_filepath);
    if (!fn) {
        return false;
    }
    auto &config = c->fn_scopes[fn]->locals;

    DEFINE(corelib_dir_str, getConfigValue<char const *>(c, "corelib_dir", config));
    c->corelib_dir = corelib_dir_str;
    NK_LOG_DBG("corelib_dir=`%.*s`", (int)c->corelib_dir.size(), c->corelib_dir.c_str());

    DEFINE(libc_name_str, getConfigValue<char const *>(c, "libc_name", config));
    c->libc_name = libc_name_str;
    NK_LOG_DBG("libc_name=`%.*s`", (int)c->libc_name.size(), c->libc_name.c_str());

    DEFINE(libm_name_str, getConfigValue<char const *>(c, "libm_name", config));
    c->libm_name = libm_name_str;
    NK_LOG_DBG("libm_name=`%.*s`", (int)c->libm_name.size(), c->libm_name.c_str());

    DEFINE(c_compiler_str, getConfigValue<char const *>(c, "c_compiler", config));
    c->c_compiler = c_compiler_str;
    NK_LOG_DBG("c_compiler=`%.*s`", (int)c->c_compiler.size(), c->c_compiler.c_str());

    DEFINE(c_compiler_flags_str, getConfigValue<char const *>(c, "c_compiler_flags", config));
    c->c_compiler_flags = c_compiler_flags_str;
    NK_LOG_DBG("c_compiler_flags=`%.*s`", (int)c->c_compiler_flags.size(), c->c_compiler_flags.c_str());

    return true;
}

// TODO nkl_compiler_run probably is not needed
bool nkl_compiler_run(NklCompiler c, NklAstNode root) {
    NK_PROF_FUNC();

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

bool nkl_compiler_runSrc(NklCompiler c, NkString src) {
    NK_PROF_FUNC();

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

bool nkl_compiler_runFile(NklCompiler c, NkString path) {
    NK_PROF_FUNC();

    auto prev_compiler = s_compiler;
    s_compiler = c;
    defer {
        s_compiler = prev_compiler;
    };

    DEFINE(fn, nkl_compileFile(c, fs::path{nk_s2stdView(path)}));
    nkir_invoke({&fn, nkir_functGetType(fn)}, {}, {});

    return true;
}
