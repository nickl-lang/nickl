#include "nkl/core/compiler.hpp"

#include "nk/common/hashmap.hpp"
#include "nk/common/profiler.hpp"

#define CHECK(EXPR)         \
    EXPR;                   \
    if (m_error_occurred) { \
        return {};          \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))

#define ASSIGN(SLOT, VAL)  \
    {                      \
        DEFINE(__val, VAL) \
        SLOT = __val;      \
    }

#define APPEND(AR, VAL)    \
    {                      \
        DEFINE(__val, VAL) \
        AR.push() = __val; \
    }

namespace nkl {

namespace {

LOG_USE_SCOPE(nkl::compiler)

using namespace nk::vm;

struct CompileEngine {
    node_ref_t m_root;
    ir::ProgramBuilder &m_builder;
    string &m_err;

    HashMap<Id, type_t> const &intrinsics;

    bool m_error_occurred = false;

    enum EDeclType {
        Decl_Undefined,

        Decl_Local,
        Decl_Global,
        Decl_Funct,
        Decl_ExtFunct,
        Decl_Intrinsic,
        Decl_Arg,
    };

    struct Decl {
        union {
            struct {
                ir::Local id;
                type_t type;
            } local;
            struct {
                ir::Global id;
                type_t type;
            } global;
            struct {
                ir::FunctId id;
                type_t ret_t;
                type_t args_t;
            } funct;
            struct {
                ir::ExtFunctId id;
            } ext_funct;
            struct {
                type_t fn;
            } intrinsic;
            struct {
                size_t index;
                type_t type;
            } arg;
        } as;
        EDeclType decl_type;
        bool is_const;
    };

    struct ScopeCtx {
        HashMap<Id, Decl> locals{};
    };

    struct FunctCtx {
        Id name;
        ir::FunctId id;
        node_ref_t root;
        type_t ret_t;
        type_t args_t;
        Array<Id> arg_names;
    };

    Array<ScopeCtx> scopes{};
    Array<FunctCtx> functs{};
    bool is_top_level{};

    enum EValueType : uint8_t {
        v_none = 0,

        v_val,
        v_ref,
        v_instr,
        v_decl,
    };

    struct ValueInfo {
        union Var {
            void *val;
            ir::Ref ref;
            ir::Instr instr;
            Decl decl;
        } as;
        type_t type;
        EValueType value_type;
    };

    ScopeCtx &curScope() const {
        assert(scopes.size && "no current scope");
        return scopes.back();
    }

    void pushScope() {
        LOG_DBG("entering scope=%lu", scopes.size)

        scopes.push() = {};
        curScope().locals.init();
    }

    void popScope() {
        LOG_DBG("exiting scope=%lu", scopes.size - 1)

        auto &scope = scopes.pop();
        scope.locals.deinit();
    }

    void compile() {
        DEFER({
            assert(!scopes.size && "unhandled scopes on the stack");
            scopes.deinit();
            functs.deinit();
        })

        is_top_level = true;

        m_builder.startFunct(
            m_builder.makeFunct(), cs2s(":top"), type_get_void(), type_get_tuple({}));
        m_builder.startBlock(m_builder.makeLabel(), cs2s("start"));

        pushScope();
        DEFER({ popScope(); })

        compileNode(m_root);

        gen(m_builder.make_ret());

        is_top_level = false;

        while (functs.size) {
            auto &funct = functs.pop();

            m_builder.startFunct(funct.id, id2s(funct.name), funct.ret_t, funct.args_t);
            m_builder.startBlock(m_builder.makeLabel(), cs2s("start"));

            pushScope();
            DEFER({ popScope(); })

            for (size_t i = 0; Id name : funct.arg_names) {
                auto type = type_tuple_typeAt(funct.args_t, i);
                defineArg(name, i, type);
                i++;
            }

            compileNode(funct.root);

            gen(m_builder.make_ret());

            funct.arg_names.deinit();
        }
    }

    int compileNode(node_ref_t node) {
        CHECK(compileStmt(node));
        return 0;
    }

    int compileNodeArray(NodeArray nodes) {
        for (auto const &node : nodes) {
            CHECK(compileStmt(&node));
        }
        return 0;
    }

    void compileStmt(node_ref_t node) {
        auto val = compile(node);
        auto ref = makeRef(val);
        if (val.value_type != v_none) {
            (void)ref;
            //@Todo Inspect refs separately
            // LOG_DBG("value ignored: %s", m_builder.inspect(ref))
            LOG_DBG("value ignored...")
        }
    }

    ValueInfo compile(node_ref_t node) {
        LOG_DBG("node: %s", s_ast_node_names[node->id]);

        switch (node->id) {
        case Node_nop:
            gen(m_builder.make_nop());
            return makeVoid();

        case Node_i8:
            return makeValue<type_t>(type_get_typeref(), type_get_numeric(Int8));
        case Node_i16:
            return makeValue<type_t>(type_get_typeref(), type_get_numeric(Int16));
        case Node_i32:
            return makeValue<type_t>(type_get_typeref(), type_get_numeric(Int32));
        case Node_i64:
            return makeValue<type_t>(type_get_typeref(), type_get_numeric(Int64));
        case Node_u8:
            return makeValue<type_t>(type_get_typeref(), type_get_numeric(Uint8));
        case Node_u16:
            return makeValue<type_t>(type_get_typeref(), type_get_numeric(Uint16));
        case Node_u32:
            return makeValue<type_t>(type_get_typeref(), type_get_numeric(Uint32));
        case Node_u64:
            return makeValue<type_t>(type_get_typeref(), type_get_numeric(Uint64));
        case Node_f32:
            return makeValue<type_t>(type_get_typeref(), type_get_numeric(Float32));
        case Node_f64:
            return makeValue<type_t>(type_get_typeref(), type_get_numeric(Float64));

        case Node_typeref:
            return makeValue<type_t>(type_get_typeref(), type_get_typeref());
        case Node_void:
            return makeValue<type_t>(type_get_typeref(), type_get_void());

        case Node_true:
            return makeValue<int8_t>(type_get_numeric(Int8), int8_t{1});
        case Node_false:
            return makeValue<int8_t>(type_get_numeric(Int8), int8_t{0});

        case Node_addr: {
            DEFINE(arg, compile(node->as.unary.arg));
            return makeInstr(m_builder.make_lea({}, makeRef(arg)), type_get_ptr(arg.type));
        }
        case Node_deref: {
            DEFINE(arg, compile(node->as.unary.arg));
            if (arg.type->typeclass_id != Type_Ptr) {
                return error("pointer expected in deref"), ValueInfo{};
            }
            auto ref = makeRef(arg).deref();
            return {{.ref = ref}, ref.type, v_ref};
        }

        case Node_compl: {
            DEFINE(arg, compile(node->as.unary.arg));
            return makeInstr(m_builder.make_compl({}, makeRef(arg)), arg.type);
        }
        case Node_not: {
            DEFINE(arg, compile(node->as.unary.arg));
            return makeInstr(m_builder.make_not({}, makeRef(arg)), arg.type);
        }
        case Node_uminus: {
            DEFINE(arg, compile(node->as.unary.arg));
            static constexpr size_t c_zero = 0;
            return makeInstr(
                m_builder.make_sub(
                    {}, m_builder.makeConstRef({(void *)&c_zero, arg.type}), makeRef(arg)),
                arg.type);
        }
        case Node_uplus: {
            DEFINE(arg, compile(node->as.unary.arg));
            return arg;
        }

        case Node_return: {
            DEFINE(arg, compile(node->as.unary.arg));
            gen(m_builder.make_mov(m_builder.makeRetRef(), makeRef(arg)));
            gen(m_builder.make_ret());
            return makeVoid();
        }

        case Node_ptr_type: {
            DEFINE(target_type, compile(node->as.unary.arg));
            if (target_type.type->typeclass_id != Type_Typeref) {
                return error("type expected in return type"), ValueInfo{};
            }
            return makeValue<type_t>(
                type_get_typeref(), type_get_ptr(val_as(type_t, asValue(target_type))));
        }

        case Node_add: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot add values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_add({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_sub: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot sub values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_sub({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_mul: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot mul values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_mul({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_div: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot div values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_div({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_mod: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot mod values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_mod({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }

        case Node_bitand: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot bitand values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_bitand({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_bitor: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot bitor values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_bitor({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_xor: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot xor values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_xor({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_lsh: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot lsh values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_lsh({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_rsh: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot rsh values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_rsh({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }

        case Node_and: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot and values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_and({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_or: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot or values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_or({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }

        case Node_ge: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot compare values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_ge({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_gt: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot compare values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_gt({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_le: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot compare values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_le({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_lt: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot compare values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_lt({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }

        case Node_eq: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot compare values of different types"), ValueInfo{};
            }
            return makeInstr(
                m_builder.make_eq({}, makeRef(lhs), makeRef(rhs)), type_get_numeric(Int8));
        }
        case Node_ne: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot compare values of different types"), ValueInfo{};
            }
            return makeInstr(
                m_builder.make_ne({}, makeRef(lhs), makeRef(rhs)), type_get_numeric(Int8));
        }

        case Node_array_type: {
            DEFINE(elem_type, compile(node->as.binary.lhs));
            DEFINE(elem_count, compile(node->as.binary.rhs));
            if (elem_type.value_type != v_val) {
                return error("value expected in array type"), ValueInfo{};
            }
            if (elem_type.type->typeclass_id != Type_Typeref) {
                return error("type expected in array type"), ValueInfo{};
            }
            if (elem_count.value_type != v_val) {
                return error("value expected in array type size"), ValueInfo{};
            }
            if (elem_count.type->typeclass_id != Type_Numeric) {
                return error("u64 expected in array type size"), ValueInfo{};
            }
            if (elem_count.type->as.num.value_type != Int64) {
                return error("i64 expected in array type size"), ValueInfo{};
            }
            return makeValue<type_t>(
                type_get_typeref(),
                type_get_array(
                    val_as(type_t, asValue(elem_type)), val_as(int64_t, asValue(elem_count))));
        }

        case Node_assign: {
            switch (node->as.binary.lhs->id) {
            case Node_id: {
                auto name_str = node->as.binary.lhs->as.token.val->text;
                Id name = s2id(name_str);
                DEFINE(rhs, compile(node->as.binary.rhs));
                auto res = resolve(name);
                ir::Ref ref;
                type_t type;
                switch (res.decl_type) {
                case Decl_Local:
                    if (res.as.local.type->id != rhs.type->id) {
                        return error("cannot assign values of different types"), ValueInfo{};
                    }
                    ref = m_builder.makeFrameRef(res.as.local.id);
                    type = res.as.local.type;
                    break;
                case Decl_Global:
                    if (res.as.local.type->id != rhs.type->id) {
                        return error("cannot assign values of different types"), ValueInfo{};
                    }
                    ref = m_builder.makeGlobalRef(res.as.global.id);
                    type = res.as.global.type;
                    break;
                case Decl_Undefined:
                    return error("`%.*s` is not defined", name_str.size, name_str.data),
                           ValueInfo{};
                case Decl_Funct:
                case Decl_ExtFunct:
                case Decl_Intrinsic:
                case Decl_Arg:
                    return error("cannot assign to `%.*s`", name_str.size, name_str.data),
                           ValueInfo{};
                default:
                    LOG_ERR("unknown decl type")
                    assert(!"unreachable");
                    return {};
                }
                return makeInstr(m_builder.make_mov(ref, makeRef(rhs)), type);
            }
            case Node_index:
            case Node_tuple_index: {
                DEFINE(lhs, compile(node->as.binary.lhs));
                auto type = lhs.type;
                DEFINE(value, compile(node->as.binary.rhs));
                if (value.type->id != type->id) {
                    return error("cannot assign values of different types"), ValueInfo{};
                }
                return makeInstr(m_builder.make_mov(makeRef(lhs), makeRef(value)), type);
            }
            default:
                LOG_ERR("invalid assignment")
                assert(!"unreachable");
                return {};
            }
        }

        case Node_cast: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            DEFINE(rhs, compile(node->as.binary.rhs));
            if (lhs.type->typeclass_id != Type_Typeref) {
                return error("type expected in cast"), ValueInfo{};
            }
            return makeInstr(
                m_builder.make_cast({}, makeRef(lhs), makeRef(rhs)), val_as(type_t, asValue(lhs)));
        }

        case Node_colon_assign: {
            switch (node->as.binary.lhs->id) {
            case Node_id: {
                Id name = s2id(node->as.binary.lhs->as.token.val->text);
                DEFINE(rhs, compile(node->as.binary.rhs));
                ir::Ref ref;
                if (is_top_level) {
                    auto var = m_builder.makeGlobalVar(rhs.type);
                    defineGlobal(name, var, rhs.type);
                    ref = m_builder.makeGlobalRef(var);
                } else {
                    auto var = m_builder.makeLocalVar(rhs.type);
                    defineLocal(name, var, rhs.type);
                    ref = m_builder.makeFrameRef(var);
                }
                gen(m_builder.make_mov(ref, makeRef(rhs)));
                return makeVoid();
            }
            default:
                LOG_ERR("invalid colon assignment")
                assert(!"unreachable");
                return {};
            }
        }

        case Node_index: {
            DEFINE(ar, compile(node->as.binary.lhs));
            if (ar.type->typeclass_id != Type_Array) {
                return error("array expected in array index"), ValueInfo{};
            }
            DEFINE(index, compile(node->as.binary.rhs));
            if (index.type->typeclass_id != Type_Numeric ||
                index.type->as.num.value_type != Int64) {
                return error("i64 expected in array index"), ValueInfo{};
            }
            auto type = ar.type->as.arr.elem_type;
            auto u64_t = index.type;
            auto ptr_t = type_get_ptr(type);
            auto tmp0 = m_builder.makeFrameRef(m_builder.makeLocalVar(ptr_t));
            auto tmp1 = m_builder.makeFrameRef(m_builder.makeLocalVar(ptr_t));
            gen(m_builder.make_mul(
                tmp0.as(index.type),
                makeRef(index),
                m_builder.makeConstRef(uint64_t{type->size}, u64_t)));
            gen(m_builder.make_lea(tmp1, makeRef(ar)));
            gen(m_builder.make_add(tmp0.as(u64_t), tmp0.as(u64_t), tmp1.as(u64_t)));
            return {{.ref = tmp0.deref()}, type, v_ref};
        }

        case Node_tuple_index: {
            DEFINE(lhs, compile(node->as.binary.lhs));
            type_t tuple_t = lhs.type;
            bool is_indirect = false;
            if (lhs.type->typeclass_id == Type_Ptr &&
                lhs.type->as.ptr.target_type->typeclass_id == Type_Tuple) {
                tuple_t = lhs.type->as.ptr.target_type;
                is_indirect = true;
            } else if (tuple_t->typeclass_id != Type_Tuple) {
                return error("tuple expected in tuple index"), ValueInfo{};
            }
            DEFINE(index_val, compile(node->as.binary.rhs));
            if (index_val.value_type != v_val) {
                return error("value expected in tuple index"), ValueInfo{};
            }
            if (index_val.type->typeclass_id != Type_Numeric ||
                index_val.type->as.num.value_type != Int64) {
                return error("i64 expected in tuple index"), ValueInfo{};
            }
            size_t index = val_as(int64_t, asValue(index_val));
            type_t type = type_tuple_typeAt(tuple_t, index);
            size_t offset = type_tuple_offsetAt(tuple_t, index);
            auto ref = makeRef(lhs);
            if (is_indirect) {
                ref = ref.deref();
            }
            ref = ref.plus(offset, type);
            return {{.ref = ref}, type, v_ref};
        }

        case Node_while: {
            auto l_loop = m_builder.makeLabel();
            auto l_endloop = m_builder.makeLabel();
            m_builder.startBlock(l_loop, cs2s("loop"));
            gen(m_builder.make_enter());
            DEFINE(cond, compile(node->as.binary.lhs));
            gen(m_builder.make_jmpz(makeRef(cond), l_endloop));
            CHECK(compileNode(node->as.binary.rhs));
            gen(m_builder.make_leave());
            gen(m_builder.make_jmp(l_loop));
            m_builder.startBlock(l_endloop, cs2s("endloop"));
            return makeVoid();
        }

        case Node_if: {
            auto l_endif = m_builder.makeLabel();
            ir::BlockId l_else;
            if (node->as.ternary.arg3->id != Node_none) {
                l_else = m_builder.makeLabel();
            } else {
                l_else = l_endif;
            }
            DEFINE(cond, compile(node->as.ternary.arg1));
            gen(m_builder.make_jmpz(makeRef(cond), l_else));
            CHECK(compileNode(node->as.ternary.arg2));
            if (node->as.ternary.arg3->id != Node_none) {
                gen(m_builder.make_jmp(l_endif));
                m_builder.startBlock(l_else, cs2s("else"));
                CHECK(compileNode(node->as.ternary.arg3));
            }
            m_builder.startBlock(l_endif, cs2s("endif"));
            return makeVoid();
        }

        case Node_block:
            compileNodeArray(node->as.array.nodes);
            return makeVoid();

        case Node_tuple_type: {
            auto types = Array<type_t>::create(node->as.array.nodes.size);
            DEFER({ types.deinit(); })
            for (auto const &node : node->as.array.nodes) {
                DEFINE(type, compile(&node));
                if (type.value_type != v_val) {
                    return error("value expected in tuple type"), ValueInfo{};
                }
                if (type.type->typeclass_id != Type_Typeref) {
                    return error("type expected in tuple type"), ValueInfo{};
                }
                types.push() = val_as(type_t, asValue(type));
            }
            return makeValue<type_t>(type_get_typeref(), type_get_tuple(types));
        }

        case Node_id: {
            string name_str = node->as.token.val->text;
            Id name = s2id(name_str);
            auto res = resolve(name);
            switch (res.decl_type) {
            case Decl_Undefined:
                return error("`%.*s` is not defined", name_str.size, name_str.data), ValueInfo{};
            case Decl_Local:
                return {{.decl = res}, res.as.local.type, v_decl};
            case Decl_Global:
                return {{.decl = res}, res.as.global.type, v_decl};
            case Decl_Funct:
                return {{.decl = res}, {}, v_decl};
            case Decl_ExtFunct:
                return {{.decl = res}, {}, v_decl};
            case Decl_Intrinsic:
                return {{.decl = res}, {}, v_decl};
            case Decl_Arg:
                return {{.decl = res}, res.as.arg.type, v_decl};
            default:
                LOG_ERR("unknown decl type");
                assert(!"unreachable");
                return {};
            }
        }
        case Node_numeric_float: {
            double value = 0;
            //@Todo Replace sscanf in Compiler
            int res = std::sscanf(node->as.token.val->text.data, "%lf", &value);
            (void)res;
            assert(res > 0 && res != EOF && "integer constant parsing failed");
            return makeValue<double>(type_get_numeric(Float64), value);
        }
        case Node_numeric_int: {
            int64_t value = 0;
            //@Todo Replace sscanf in Compiler
            int res = std::sscanf(node->as.token.val->text.data, "%ld", &value);
            (void)res;
            assert(res > 0 && res != EOF && "integer constant parsing failed");
            return makeValue<int64_t>(type_get_numeric(Int64), value);
        }
        case Node_string_literal: {
            string val = string_format(
                m_builder.prog->arena,
                "%.*s",
                node->as.token.val->text.size,
                node->as.token.val->text.data);
            type_t ar_t = type_get_array(type_get_numeric(Int8), val.size);
            return makeValue<void const *>(type_get_ptr(ar_t), val.data);
        }
        case Node_escaped_string_literal: {
            string val = string_unescape(m_builder.prog->arena, node->as.token.val->text);
            type_t ar_t = type_get_array(type_get_numeric(Int8), val.size);
            return makeValue<void const *>(type_get_ptr(ar_t), val.data);
        }

        case Node_fn: {
            auto arg_types = Array<type_t>::create(node->as.fn.sig.params.size);
            auto arg_names = Array<Id>::create(node->as.fn.sig.params.size);
            // NOTE: arg_names is freed afters the function has been compiled
            //@Fix if compiler encounteds an error before, arg_names leaks
            DEFER({ arg_types.deinit(); })
            for (auto const &arg : node->as.fn.sig.params) {
                DEFINE(arg_t, compile(arg.as.named_node.node));
                if (arg_t.type->typeclass_id != Type_Typeref) {
                    return error("type expected in arguments"), ValueInfo{};
                }
                arg_types.push() = val_as(type_t, asValue(arg_t));
                arg_names.push() = s2id(arg.as.named_node.name->text);
            }
            DEFINE(ret_t_info, compile(node->as.fn.sig.ret_type));
            if (ret_t_info.type->typeclass_id != Type_Typeref) {
                return error("type expected in return type"), ValueInfo{};
            }
            Id name = s2id(node->as.fn.sig.name->text);
            auto funct_id = m_builder.makeFunct();
            auto ret_t = val_as(type_t, asValue(ret_t_info));
            auto args_t = type_get_tuple(arg_types.slice());
            defineFunct(name, funct_id, ret_t, args_t);
            functs.push() = {
                .name = name,
                .id = funct_id,
                .root = node->as.fn.body,
                .ret_t = ret_t,
                .args_t = args_t,
                .arg_names = arg_names,
            };
            return makeVoid();
        }

        case Node_foreign_fn: {
            auto arg_types = Array<type_t>::create(node->as.fn.sig.params.size);
            DEFER({ arg_types.deinit(); })
            for (auto const &arg : node->as.fn.sig.params) {
                DEFINE(arg_t, compile(arg.as.named_node.node));
                if (arg_t.type->typeclass_id != Type_Typeref) {
                    return error("type expected in arguments"), ValueInfo{};
                }
                arg_types.push() = val_as(type_t, asValue(arg_t));
            }
            DEFINE(ret_t, compile(node->as.fn.sig.ret_type));
            if (ret_t.type->typeclass_id != Type_Typeref) {
                return error("type expected in return type"), ValueInfo{};
            }
            Id name = s2id(node->as.fn.sig.name->text);
            auto id = m_builder.makeExtFunct(
                m_builder.makeShObj(s2id(node->as.fn.lib->text)),
                name,
                val_as(type_t, asValue(ret_t)),
                type_get_tuple(arg_types.slice()),
                node->as.fn.is_variadic);
            defineExtFunct(name, id);
            return makeVoid();
        }

        case Node_call: {
            DEFINE(lhs, compile(node->as.call.lhs));
            auto const make_args = [&](type_t args_t, bool is_variadic = false) -> ir::Ref {
                auto arg_types = Array<type_t>::create(node->as.call.args.size);
                DEFER({ arg_types.deinit(); })
                auto args_info = Array<ValueInfo>::create(node->as.call.args.size);
                DEFER({ args_info.deinit(); })
                for (auto const &arg_node : node->as.call.args) {
                    DEFINE(arg, compile(&arg_node));
                    //@Incomplete Check call arg types in Compiler
                    args_info.push() = arg;
                    arg_types.push() = arg.type;
                }
                auto actual_args_t = type_get_tuple(arg_types);
                auto args = m_builder.makeFrameRef(m_builder.makeLocalVar(actual_args_t));
                for (size_t i = 0; auto const &arg : args_info) {
                    gen(m_builder.make_mov(
                        args.plus(
                            type_tuple_offsetAt(actual_args_t, i),
                            type_tuple_typeAt(actual_args_t, i)),
                        makeRef(arg)));
                    i++;
                }
                return args;
            };
            if (lhs.value_type != v_decl) {
                return error("call through a pointer is not implemented"), ValueInfo{};
            }
            auto &decl = lhs.as.decl;
            switch (decl.decl_type) {
            case Decl_Local:
            case Decl_Global:
            case Decl_Arg:
                return error("call through a pointer is not implemented"), ValueInfo{};
            case Decl_Funct: {
                auto const &f = decl.as.funct;
                return makeInstr(
                    m_builder.make_call({}, decl.as.funct.id, make_args(f.args_t)), f.ret_t);
            }
            case Decl_ExtFunct: {
                auto const &f = m_builder.prog->exsyms[decl.as.ext_funct.id.id].as.funct;
                return makeInstr(
                    m_builder.make_call(
                        {}, decl.as.ext_funct.id, make_args(f.args_t, f.is_variadic)),
                    f.ret_t);
            }
            case Decl_Intrinsic: {
                auto const &f = decl.as.intrinsic.fn->as.fn;
                return makeInstr(
                    m_builder.make_call(
                        {},
                        m_builder.makeConstRef({nullptr, decl.as.intrinsic.fn}),
                        make_args(f.args_t)),
                    f.ret_t);
            }
            case Decl_Undefined:
            default:
                assert(!"unreachable");
                return {};
            }
            break;
        }

        case Node_var_decl: {
            Id name = s2id(node->as.var_decl.name->text);
            DEFINE(type_val, compile(node->as.var_decl.type));
            if (type_val.type->typeclass_id != Type_Typeref) {
                return error("type expected in variable declaration"), ValueInfo{};
            }
            type_t type = val_as(type_t, asValue(type_val));
            ir::Ref ref;
            if (is_top_level) {
                auto var = m_builder.makeGlobalVar(type);
                defineGlobal(name, var, type);
                ref = m_builder.makeGlobalRef(var);
            } else {
                auto var = m_builder.makeLocalVar(type);
                defineLocal(name, var, type);
                ref = m_builder.makeFrameRef(var);
            }
            if (node->as.var_decl.value->id != Node_none) {
                DEFINE(val, compile(node->as.var_decl.value));
                gen(m_builder.make_mov(ref, makeRef(val)));
            }
            return makeVoid();
        }

        default:
            return error("unknown node '%s'", s_ast_node_names[node->id]), ValueInfo{};
        }

        return {};
    }

    ValueInfo makeVoid() {
        return {{}, type_get_void(), v_none};
    }

    Decl &makeDecl(Id name) {
        auto &locals = curScope().locals;

        LOG_DBG("making declaration %s", [&]() {
            string str = id2s(name);
            return tmpstr_format("name=`%.*s` scope=%lu", str.size, str.data, scopes.size - 1).data;
        }());

        auto found = locals.find(name);
        if (found) {
            static Decl s_dummy_decl;
            string name_str = id2s(name);
            return error("`%.*s` is already defined", name_str.size, name_str.data), s_dummy_decl;
        }

        return locals.insert(name) = {};
    }

    void defineLocal(Id name, ir::Local id, type_t type) {
        LOG_DBG("defining local `%s`", [&]() {
            string str = id2s(name);
            return tmpstr_format("%.*s", str.size, str.data).data;
        }());
        makeDecl(name) = {{.local = {id, type}}, Decl_Local, false};
    }

    void defineGlobal(Id name, ir::Global id, type_t type) {
        LOG_DBG("defining global `%s`", [&]() {
            string str = id2s(name);
            return tmpstr_format("%.*s", str.size, str.data).data;
        }());
        makeDecl(name) = {{.global = {id, type}}, Decl_Global, false};
    }

    void defineFunct(Id name, ir::FunctId id, type_t ret_t, type_t args_t) {
        LOG_DBG("defining funct `%s`", [&]() {
            string str = id2s(name);
            return tmpstr_format("%.*s", str.size, str.data).data;
        }());
        makeDecl(name) = {
            {.funct = {.id = id, .ret_t = ret_t, .args_t = args_t}}, Decl_Funct, false};
    }

    void defineExtFunct(Id name, ir::ExtFunctId id) {
        LOG_DBG("defining ext funct `%s`", [&]() {
            string str = id2s(name);
            return tmpstr_format("%.*s", str.size, str.data).data;
        }());
        makeDecl(name) = {{.ext_funct = {id}}, Decl_ExtFunct, false};
    }

    void defineArg(Id name, size_t index, type_t type) {
        LOG_DBG("defining arg `%s`", [&]() {
            string str = id2s(name);
            return tmpstr_format("%.*s", str.size, str.data).data;
        }());
        makeDecl(name) = {{.arg = {index, type}}, Decl_Arg, true};
    }

    Decl resolve(Id name) {
        LOG_DBG("resolving %s", [&]() {
            string str = id2s(name);
            return tmpstr_format("name=`%.*s` scope=%lu", str.size, str.data, scopes.size - 1).data;
        }());

        for (size_t i = scopes.size; i > 0; i--) {
            auto &scope = scopes[i - 1];
            auto found = scope.locals.find(name);
            if (found) {
                return *found;
            }
        }
        auto found = intrinsics.find(name);
        if (found) {
            return {{.intrinsic = {*found}}, Decl_Intrinsic, true};
        }
        return {{}, Decl_Undefined, false};
    }

    template <class T, class... TArgs>
    ValueInfo makeValue(type_t type, TArgs &&...args) {
        auto mem = _mctx.tmp_allocator->alloc<T>();
        *mem = T{args...};
        return {{.val = mem}, type, v_val};
    }

    ValueInfo makeInstr(ir::Instr const &instr, type_t type) {
        return {{.instr{instr}}, type, v_instr};
    }

    value_t asValue(ValueInfo &val) {
        return {val.as.val, val.type};
    }

    ir::Ref makeRef(ValueInfo const &val) {
        switch (val.value_type) {
        case v_val:
            return m_builder.makeConstRef({val.as.val, val.type});

        case v_ref:
            return val.as.ref;

        case v_instr: {
            auto instr = val.as.instr;
            auto &ref = instr.arg[0].as.ref;
            if (ref.ref_type == ir::Ref_None && val.type->typeclass_id != Type_Void) {
                ref = m_builder.makeFrameRef(m_builder.makeLocalVar(val.type));
            }
            gen(instr);
            return ref;
        }

        case v_decl:
            switch (val.as.decl.decl_type) {
            case Decl_Local:
                return m_builder.makeFrameRef(val.as.decl.as.local.id);
            case Decl_Global:
                return m_builder.makeGlobalRef(val.as.decl.as.global.id);
            case Decl_Funct:
                LOG_ERR("Funct ref is not implemented");
                assert(!"unreachable");
                return {};
            case Decl_ExtFunct:
                LOG_ERR("Ext funct ref is not implemented");
                assert(!"unreachable");
                return {};
            case Decl_Arg:
                return m_builder.makeArgRef(val.as.decl.as.arg.index);
            case Decl_Undefined:
            default:
                assert(!"unreachable");
                return {};
            }

        default:
            return {};
        };
    }

    void gen(ir::Instr const &instr) {
        EASY_FUNCTION(::profiler::colors::Cyan200)

        LOG_DBG("gen: %s", ir::s_ir_names[instr.code])

        m_builder.gen(instr);
    }

    void error(char const *fmt, ...) {
        assert(!m_error_occurred && "Compiler error already initialized");
        m_error_occurred = true;
        va_list ap;
        va_start(ap, fmt);
        m_err = tmpstr_vformat(fmt, ap);
        va_end(ap);
    }
};

} // namespace

void Compiler::init() {
    *this = {};
}

void Compiler::deinit() {
    intrinsics.deinit();
    prog.deinit();
}

bool Compiler::compile(node_ref_t root) {
    EASY_FUNCTION(::profiler::colors::Cyan200);

    prog.init();

    ir::ProgramBuilder builder{};
    builder.init(prog);

    CompileEngine engine{root, builder, err, intrinsics};
    engine.compile();

    LOG_INF("%s", [&]() {
        auto str = prog.inspect();
        str = tmpstr_format("ir:\n%.*s", str.size, str.data);
        return str.data;
    }());

    return !engine.m_error_occurred;
}

} // namespace nkl
