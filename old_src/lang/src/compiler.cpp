#include "nkl/lang/compiler.hpp"

#include "nk/str/dynamic_string_builder.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/profiler.hpp"
#include "nkl/lang/compiler_api.hpp"

#define CHECK(EXPR)         \
    EXPR;                   \
    if (m_error_occurred) { \
        return {};          \
    }

#define DEFINE(VAR, VAL) CHECK(auto VAR = (VAL))
#define ASSIGN(SLOT, VAL) CHECK(SLOT = (VAL))
#define APPEND(AR, VAL) CHECK(*AR.push() = (VAL))

namespace nkl {

namespace {

using namespace nk::vm;

LOG_USE_SCOPE(nkl::compiler);

struct CompileEngine {
    NodeRef m_root;
    ir::ProgramBuilder &m_builder;
    StringBuilder &m_err;

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
                value_t value;
                bool is_const;
            } local;
            struct {
                ir::Global id;
                type_t type;
                value_t value;
                bool is_const;
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
    };

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

    struct Void {};

    struct ScopeCtx {
        HashMap<Id, Decl> locals{};
    };

    StackAllocator m_arena{};
    Array<ScopeCtx> m_scopes{};
    bool m_is_top_level = true;

    ScopeCtx &curScope() const {
        assert(m_scopes.size && "no current scope");
        return m_scopes.back();
    }

    void pushScope() {
        LOG_DBG("entering scope=%lu", m_scopes.size);

        *m_scopes.push() = {};
    }

    void popScope() {
        LOG_DBG("exiting scope=%lu", m_scopes.size - 1);

        auto &scope = m_scopes.back();
        scope.locals.deinit();
        m_scopes.pop();
    }

    void compile() {
        m_arena.reserve(1024);
        defer {
            assert(!m_scopes.size && "unhandled scopes on the stack");
            m_scopes.deinit();
            m_arena.deinit();
        };

        m_is_top_level = true;

        m_builder.startFunct(
            m_builder.makeFunct(), cs2s(":top"), types::get_void(), types::get_tuple({}));
        m_builder.startBlock(m_builder.makeLabel(), cs2s("start"));

        pushScope();
        defer {
            popScope();
        };

        compileNode(m_root);

        gen(m_builder.make_ret());

        m_is_top_level = false;
    }

    int compileNode(NodeRef node) {
        CHECK(compileStmt(node));
        return 0;
    }

    int compileNodeArray(NodeArray nodes) {
        for (auto const &node : nodes) {
            CHECK(compileStmt(&node));
        }
        return 0;
    }

    void compileStmt(NodeRef node) {
        auto val = compile(node);
        auto ref = makeRef(val);
        if (val.value_type != v_none) {
            (void)ref;
            //@Boilerplate for debug printing
#ifdef ENABLE_LOGGING
            StackAllocator arena{};
            defer {
                arena.deinit();
            };
            DynamicStringBuilder sb{};
            defer {
                sb.deinit();
            };
#endif // ENABLE_LOGGING
            LOG_DBG("value ignored: %s", [&]() {
                return m_builder.prog.inspectRef(ref, sb).moveStr(arena).data;
            }());
        }
    }

    ValueInfo compile(NodeRef node) {
        LOG_DBG("node: %s", s_ast_node_names[node->id]);

        switch (node->id) {
        case Node_nop:
            gen(m_builder.make_nop());
            return makeVoid();

        case Node_i8:
            return makeValue<type_t>(types::get_type(), types::get_numeric(Int8));
        case Node_i16:
            return makeValue<type_t>(types::get_type(), types::get_numeric(Int16));
        case Node_i32:
            return makeValue<type_t>(types::get_type(), types::get_numeric(Int32));
        case Node_i64:
            return makeValue<type_t>(types::get_type(), types::get_numeric(Int64));
        case Node_u8:
            return makeValue<type_t>(types::get_type(), types::get_numeric(Uint8));
        case Node_u16:
            return makeValue<type_t>(types::get_type(), types::get_numeric(Uint16));
        case Node_u32:
            return makeValue<type_t>(types::get_type(), types::get_numeric(Uint32));
        case Node_u64:
            return makeValue<type_t>(types::get_type(), types::get_numeric(Uint64));
        case Node_f32:
            return makeValue<type_t>(types::get_type(), types::get_numeric(Float32));
        case Node_f64:
            return makeValue<type_t>(types::get_type(), types::get_numeric(Float64));

        case Node_any_t:
            return makeValue<type_t>(types::get_type(), types::get_any());
        case Node_bool:
            return makeValue<type_t>(types::get_type(), types::get_bool());
        case Node_type_t:
            return makeValue<type_t>(types::get_type(), types::get_type());
        case Node_void:
            return makeValue<type_t>(types::get_type(), types::get_void());

        case Node_true:
            return makeValue<int8_t>(types::get_numeric(Int8), int8_t{1});
        case Node_false:
            return makeValue<int8_t>(types::get_numeric(Int8), int8_t{0});

        case Node_compl: {
            DEFINE(arg, compile(Node_unary_arg(node)));
            return makeInstr(m_builder.make_compl({}, makeRef(arg)), arg.type);
        }
        case Node_not: {
            DEFINE(arg, compile(Node_unary_arg(node)));
            return makeInstr(m_builder.make_not({}, makeRef(arg)), arg.type);
        }
        case Node_uminus: {
            DEFINE(arg, compile(Node_unary_arg(node)));
            static constexpr size_t c_zero = 0;
            return makeInstr(
                m_builder.make_sub(
                    {}, m_builder.makeConstRef({(void *)&c_zero, arg.type}), makeRef(arg)),
                arg.type);
        }
        case Node_uplus: {
            DEFINE(arg, compile(Node_unary_arg(node)));
            return arg;
        }

        case Node_addr: {
            DEFINE(arg, compile(Node_unary_arg(node)));
            return makeInstr(m_builder.make_lea({}, makeRef(arg)), types::get_ptr(arg.type));
        }
        case Node_deref: {
            DEFINE(arg, compile(Node_unary_arg(node)));
            if (arg.type->typeclass_id != Type_Ptr) {
                return error("pointer expected in deref"), ValueInfo{};
            }
            auto ref = makeRef(arg).deref();
            return {{.ref = ref}, ref.type, v_ref};
        }

        case Node_return: {
            DEFINE(arg, compile(Node_unary_arg(node)));
            gen(m_builder.make_mov(m_builder.makeRetRef(), makeRef(arg)));
            gen(m_builder.make_ret());
            return makeVoid();
        }

        case Node_ptr_type: {
            DEFINE(target_type, compile(Node_unary_arg(node)));
            if (target_type.type->typeclass_id != Type_Type) {
                return error("type expected in pointer type"), ValueInfo{};
            }
            return makeValue<type_t>(
                types::get_type(), types::get_ptr(val_as(type_t, asValue(target_type))));
        }

        case Node_slice_type: {
            DEFINE(target_type, compile(Node_unary_arg(node)));
            if (target_type.type->typeclass_id != Type_Type) {
                return error("type expected in slice type"), ValueInfo{};
            }
            return makeValue<type_t>(
                types::get_type(), types::get_slice(val_as(type_t, asValue(target_type))));
        }

        case Node_scope: {
            pushScope();
            defer {
                popScope();
            };

            return compile(Node_unary_arg(node));
        }

        case Node_add: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot add values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_add({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_sub: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot sub values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_sub({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_mul: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot mul values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_mul({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_div: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot div values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_div({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_mod: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot mod values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_mod({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }

        case Node_bitand: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot bitand values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_bitand({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_bitor: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot bitor values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_bitor({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_xor: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot xor values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_xor({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_lsh: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot lsh values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_lsh({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_rsh: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot rsh values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_rsh({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }

        case Node_and: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot and values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_and({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_or: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot or values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_or({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }

        case Node_eq: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot eq values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_eq({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_ge: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot ge values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_ge({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_gt: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot gt values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_gt({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_le: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot le values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_le({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_lt: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot lt values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_lt({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }
        case Node_ne: {
            DEFINE(lhs, compile(Node_binary_lhs(node)));
            DEFINE(rhs, compile(Node_binary_rhs(node)));
            if (lhs.type->id != rhs.type->id) {
                return error("cannot ne values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_ne({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }

        case Node_while: {
            auto l_loop = m_builder.makeLabel();
            auto l_endloop = m_builder.makeLabel();
            m_builder.startBlock(l_loop, cs2s("loop"));
            gen(m_builder.make_enter());
            DEFINE(cond, compile(Node_binary_lhs(node)));
            gen(m_builder.make_jmpz(makeRef(cond), l_endloop));
            CHECK(compileNode(Node_binary_rhs(node)));
            gen(m_builder.make_leave());
            gen(m_builder.make_jmp(l_loop));
            m_builder.startBlock(l_endloop, cs2s("endloop"));
            return makeVoid();
        }

        case Node_if: {
            auto l_endif = m_builder.makeLabel();
            ir::BlockId l_else;
            if (Node_ternary_else_clause(node)->id != Node_none) {
                l_else = m_builder.makeLabel();
            } else {
                l_else = l_endif;
            }
            DEFINE(cond, compile(Node_ternary_cond(node)));
            gen(m_builder.make_jmpz(makeRef(cond), l_else));
            CHECK(compileNode(Node_ternary_then_clause(node)));
            if (Node_ternary_else_clause(node)->id != Node_none) {
                gen(m_builder.make_jmp(l_endif));
                m_builder.startBlock(l_else, cs2s("else"));
                CHECK(compileNode(Node_ternary_else_clause(node)));
            }
            m_builder.startBlock(l_endif, cs2s("endif"));
            return makeVoid();
        }

        case Node_block:
            compileNodeArray(Node_array_nodes(node));
            return makeVoid();

        case Node_id: {
            string name_str = Node_token_value(node)->text;
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
            case Decl_Arg:
                return {{.decl = res}, res.as.arg.type, v_decl};
            default:
                LOG_ERR("unknown decl type");
                assert(!"unreachable");
                return {};
            }
        }

        case Node_intrinsic: {
            string name_str = Node_token_value(node)->text;
            Id name = s2id(name_str);
            auto res = resolve(name);
            switch (res.decl_type) {
            case Decl_Undefined:
                return error("`%.*s` is not defined", name_str.size, name_str.data), ValueInfo{};
            case Decl_Intrinsic:
                return {{.decl = res}, {}, v_decl};
            default:
                LOG_ERR("unknown decl type");
                assert(!"unreachable");
                return {};
            }
        }

        case Node_numeric_float: {
            double value = 0;
            //@Todo Replace sscanf in Compiler
            int res = std::sscanf(Node_token_value(node)->text.data, "%lf", &value);
            (void)res;
            assert(res > 0 && res != EOF && "integer constant parsing failed");
            return makeValue<double>(types::get_numeric(Float64), value);
        }
        case Node_numeric_int: {
            int64_t value = 0;
            //@Todo Replace sscanf in Compiler
            int res = std::sscanf(Node_token_value(node)->text.data, "%ld", &value);
            (void)res;
            assert(res > 0 && res != EOF && "integer constant parsing failed");
            return makeValue<int64_t>(types::get_numeric(Int64), value);
        }
        case Node_string_literal: {
            string val = Node_token_value(node)->text.copy(
                m_builder.prog.arena.alloc<char>(Node_token_value(node)->text.size));
            type_t ar_t = types::get_array(types::get_numeric(Int8), val.size);
            return makeValue<void const *>(types::get_ptr(ar_t), val.data);
        }
        case Node_escaped_string_literal: {
            DynamicStringBuilder sb{};
            string_unescape(sb, Node_token_value(node)->text);
            string val = sb.moveStr(m_builder.prog.arena);
            type_t ar_t = types::get_array(types::get_numeric(Int8), val.size);
            return makeValue<void const *>(types::get_ptr(ar_t), val.data);
        }

        case Node_fn: {
            auto const &params = Node_fn_params(node);
            Array<type_t> arg_types{};
            arg_types.reserve(params.size);
            Array<Id> arg_names{};
            arg_names.reserve(params.size);
            // NOTE: arg_names is freed afters the function has been compiled
            //@Fix if compiler encounteds an error before, arg_names leaks
            defer {
                arg_types.deinit();
            };
            for (size_t i = 0; i < params.size; i++) {
                auto const &arg = params[i];
                DEFINE(arg_t, compile(arg.type));
                if (arg_t.type->typeclass_id != Type_Type) {
                    return error("type expected in arguments"), ValueInfo{};
                }
                *arg_types.push() = val_as(type_t, asValue(arg_t));
                *arg_names.push() = s2id(arg.name->text);
            }
            DEFINE(ret_t_info, compile(Node_fn_ret_type(node)));
            if (ret_t_info.type->typeclass_id != Type_Type) {
                return error("type expected in return type"), ValueInfo{};
            }
            // auto funct_id = m_builder.makeFunct();
            // auto ret_t = val_as(type_t, asValue(ret_t_info));
            // auto args_t = types::get_tuple(arg_types.slice());
            //@Todo defineFunct commented out
            // defineFunct(name, funct_id, ret_t, args_t);
            //@Todo *functs.push() commented out
            // *functs.push() = {
            //     .name = name,
            //     .id = funct_id,
            //     .root = node->as.fn.body,
            //     .ret_t = ret_t,
            //     .args_t = args_t,
            //     .arg_names = arg_names,
            // };
            //@Todo Not returning anything for Node_fn
            return makeVoid();
        }

        case Node_call: {
            DEFINE(lhs, compile(Node_call_lhs(node)));
            auto const make_args = [&](type_t args_t, bool is_variadic = false) -> ir::Ref {
                Array<type_t> arg_types{};
                arg_types.reserve(Node_call_args(node).size);
                defer {
                    arg_types.deinit();
                };
                Array<ValueInfo> args_info{};
                args_info.reserve(Node_call_args(node).size);
                defer {
                    args_info.deinit();
                };
                for (auto const &arg_node : Node_call_args(node)) {
                    DEFINE(arg, compile(&arg_node));
                    //@Incomplete Check call arg types in Compiler
                    *args_info.push() = arg;
                    *arg_types.push() = arg.type;
                }
                auto actual_args_t = types::get_tuple(arg_types);
                auto args = m_builder.makeFrameRef(m_builder.makeLocalVar(actual_args_t));
                for (size_t i = 0; auto const &arg : args_info) {
                    gen(m_builder.make_mov(
                        args.plus(
                            types::tuple_offsetAt(actual_args_t, i),
                            types::tuple_typeAt(actual_args_t, i)),
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
                auto const &f = m_builder.prog.exsyms[decl.as.ext_funct.id.id].as.funct;
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

        case Node_define: {
            auto const &names = Node_define_names(node);
            if (names.size > 1) {
                return error("multiple assignment is not implemented"), ValueInfo{};
            }
            Id name = s2id(names[0]->text);
            DEFINE(rhs, compile(Node_define_value(node)));
            ir::Ref ref;
            if (m_is_top_level) {
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

        case Node_var_decl: {
            Id name = s2id(Node_var_decl_name(node)->text);
            DEFINE(type_val, compile(Node_var_decl_type(node)));
            if (type_val.type->typeclass_id != Type_Type) {
                return error("type expected in variable declaration"), ValueInfo{};
            }
            type_t type = val_as(type_t, asValue(type_val));
            ir::Ref ref;
            if (m_is_top_level) {
                auto var = m_builder.makeGlobalVar(type);
                defineGlobal(name, var, type);
                ref = m_builder.makeGlobalRef(var);
            } else {
                auto var = m_builder.makeLocalVar(type);
                defineLocal(name, var, type);
                ref = m_builder.makeFrameRef(var);
            }
            if (Node_var_decl_value(node)->id != Node_none) {
                DEFINE(val, compile(Node_var_decl_value(node)));
                gen(m_builder.make_mov(ref, makeRef(val)));
            }
            return makeVoid();
        }

        default:
            return error("node '%s' compilation is not implemented", s_ast_node_names[node->id]),
                   ValueInfo{};
        }

        return {};
    }

    ValueInfo makeVoid() {
        return {{}, types::get_void(), v_none};
    }

    Decl &makeDecl(Id name) {
        auto &locals = curScope().locals;

        LOG_DBG(
            "making declaration name=`%.*s` scope=%lu",
            id2s(name).size,
            id2s(name).data,
            m_scopes.size - 1);

        auto found = locals.find(name);
        if (found) {
            static Decl s_dummy_decl;
            string name_str = id2s(name);
            return error("`%.*s` is already defined", name_str.size, name_str.data), s_dummy_decl;
        }

        return locals.insert(name, {});
    }

    void defineLocal(Id name, ir::Local id, type_t type) {
        LOG_DBG("defining local `%.*s`", id2s(name).size, id2s(name).data);
        makeDecl(name) = {{.local = {id, type}}, Decl_Local};
    }

    void defineGlobal(Id name, ir::Global id, type_t type) {
        LOG_DBG("defining global `%.*s`", id2s(name).size, id2s(name).data);
        makeDecl(name) = {{.global = {id, type}}, Decl_Global};
    }

    void defineFunct(Id name, ir::FunctId id, type_t ret_t, type_t args_t) {
        LOG_DBG("defining funct `%.*s`", id2s(name).size, id2s(name).data);
        makeDecl(name) = {{.funct = {.id = id, .ret_t = ret_t, .args_t = args_t}}, Decl_Funct};
    }

    void defineExtFunct(Id name, ir::ExtFunctId id) {
        LOG_DBG("defining ext funct `%.*s`", id2s(name).size, id2s(name).data);
        makeDecl(name) = {{.ext_funct = {id}}, Decl_ExtFunct};
    }

    void defineArg(Id name, size_t index, type_t type) {
        LOG_DBG("defining arg `%.*s`", id2s(name).size, id2s(name).data);
        makeDecl(name) = {{.arg = {index, type}}, Decl_Arg};
    }

    Decl resolve(Id name) {
        LOG_DBG(
            "resolving name=`%.*s` scope=%lu", id2s(name).size, id2s(name).data, m_scopes.size - 1);

        for (size_t i = m_scopes.size; i > 0; i--) {
            auto &scope = m_scopes[i - 1];
            auto found = scope.locals.find(name);
            if (found) {
                return *found;
            }
        }
        auto found = intrinsics.find(name);
        if (found) {
            return {{.intrinsic = {*found}}, Decl_Intrinsic};
        }
        return {{}, Decl_Undefined};
    }

    template <class T, class... TArgs>
    ValueInfo makeValue(type_t type, TArgs &&...args) {
        auto mem = m_arena.alloc<T>();
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
        LOG_DBG("gen: %s", ir::s_ir_names[instr.code]);
        m_builder.gen(instr);
    }

    void error(char const *fmt, ...) {
        assert(!m_error_occurred && "Compiler error already initialized");
        m_error_occurred = true;
        va_list ap;
        va_start(ap, fmt);
        m_err.vprintf(fmt, ap);
        va_end(ap);
    }
};

} // namespace

struct CompilerContext {};

struct CompilerState {
    CompilerContext m_cur_compiler;
};

bool Compiler::compile(NodeRef root) {
    EASY_FUNCTION(::profiler::colors::Cyan200);

    ir::ProgramBuilder builder{prog};

    CompileEngine engine{root, builder, err, intrinsics};
    engine.compile();

#ifdef ENABLE_LOGGING
    StackAllocator arena{};
    defer {
        arena.deinit();
    };
    DynamicStringBuilder sb{};
    defer {
        sb.deinit();
    };
#endif // ENABLE_LOGGING
    LOG_INF("%s", [&]() {
        return prog.inspect(sb).moveStr(arena).data;
    }());

    return !engine.m_error_occurred;
}

extern "C" CompilerContext *nk_compiler_getInstance() {
    static CompilerState s_instance;
    return &s_instance.m_cur_compiler;
}

extern "C" bool nk_compiler_defineVar(CompilerContext *c, string name, type_t type) {
}

extern "C" bool nk_compiler_defineVarInit(CompilerContext *c, string name, value_t value) {
}

extern "C" bool nk_compiler_defineConst(CompilerContext *c, string name, value_t value) {
}

} // namespace nkl
