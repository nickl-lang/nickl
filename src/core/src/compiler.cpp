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

    bool m_error_occurred = false;

    enum EDeclType {
        Decl_Undefined,

        Decl_Local,
        Decl_Global,
        Decl_Funct,
        Decl_ExtFunct,
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
            } funct;
            struct {
                ir::ExtFunctId id;
            } ext_funct;
        } as;
        EDeclType decl_type;
        bool is_const;
    };

    struct ScopeCtx {
        HashMap<Id, Decl> locals{};
    };

    struct FrameCtx {
        Array<ScopeCtx> scopes{};
    };

    Array<FrameCtx> frames{};

    enum EValueType : uint8_t {
        v_none = 0,

        v_val,
        v_instr,
        v_decl,
    };

    struct ValueInfo {
        union Var {
            void *val;
            ir::Instr instr;
            Decl decl;
        } as;
        type_t type;
        EValueType value_type;
    };

    FrameCtx &curFrame() const {
        assert(frames.size && "no current frame");
        return frames.back();
    }

    ScopeCtx &curScope() const {
        assert(curFrame().scopes.size && "no current scope");
        return curFrame().scopes.back();
    }

    void pushFrame() {
        frames.push() = {};
    }

    void popFrame() {
        auto &frame = frames.pop();
        assert(!frame.scopes.size && "unhandled scopes on the stack");
        frame.scopes.deinit();
    }

    void pushScope() {
        curFrame().scopes.push() = {};
        curScope().locals.init();
    }

    void popScope() {
        auto &scope = curFrame().scopes.pop();
        scope.locals.deinit();
    }

    void compile() {
        pushFrame();
        DEFER({
            popFrame();
            assert(!frames.size && "unhandled frames on the stack");
            frames.deinit();
        })

        m_builder.startFunct(
            m_builder.makeFunct(), cs2s(":top"), type_get_void(), type_get_tuple({}));
        m_builder.startBlock(m_builder.makeLabel(), cs2s("start"));

        compileScope(m_root);

        gen(m_builder.make_ret());
    }

    int compileScope(node_ref_t node) {
        pushScope();
        DEFER({ popScope(); })

        CHECK(compileStmt(node));

        return 0;
    }

    int compileScope(NodeArray nodes) {
        pushScope();
        DEFER({ popScope(); })

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
        }
    }

    ValueInfo compile(node_ref_t node) {
        LOG_DBG("node: %s", s_ast_node_names[node->id]);

        switch (node->id) {
        case Node_nop:
            gen(m_builder.make_nop());
            //@Refactor Construct void ref universally
            return {{}, type_get_void(), v_none};

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
                return error("cannot add two values of different types"), ValueInfo{};
            }
            return makeInstr(m_builder.make_add({}, makeRef(lhs), makeRef(rhs)), lhs.type);
        }

        case Node_block:
            compileScope(node->as.array.nodes);
            return {{}, type_get_void(), v_none};

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
            type_t ar_t =
                type_get_array(type_get_numeric(Uint8), node->as.token.val->text.size + 1);
            auto ref = m_builder.makeConstRef({(void *)node->as.token.val->text.data, ar_t});
            ((char *)ref.value.data)[node->as.token.val->text.size] = 0;
            return makeValue<void *>(type_get_ptr(ar_t), ref.value.data);
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
            return {{}, type_get_void(), v_none};
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
                return error("call through a pointer is not implemented"), ValueInfo{};
            case Decl_Funct: {
                auto const &f = m_builder.prog->functs[decl.as.funct.id.id];
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
            case Decl_Undefined:
            default:
                assert(!"unreachable");
                return {};
            }
            break;
        }

        default:
            return error("unknown node '%s'", s_ast_node_names[node->id]), ValueInfo{};
        }

        return {};
    }

    Decl &makeDecl(Id name) {
        auto &locals = curScope().locals;

        auto found = locals.find(name);
        if (found) {
            static Decl s_dummy_decl;
            string name_str = id2s(name);
            return error("`%.*s` is already defined", name_str.size, name_str.data), s_dummy_decl;
        }

        return locals.insert(name) = {};
    }

    void defineLocal(Id name, ir::Local id, type_t type) {
        makeDecl(name) = {{.local = {id, type}}, Decl_Local, false};
    }

    void defineGlobal(Id name, ir::Global id, type_t type) {
        makeDecl(name) = {{.global = {id, type}}, Decl_Global, false};
    }

    void defineFunct(Id name, ir::FunctId id) {
        makeDecl(name) = {{.funct = {id}}, Decl_Funct, false};
    }

    void defineExtFunct(Id name, ir::ExtFunctId id) {
        makeDecl(name) = {{.ext_funct = {id}}, Decl_ExtFunct, false};
    }

    Decl resolve(Id name) {
        for (size_t i = curFrame().scopes.size; i > 0; i--) {
            auto &scope = curFrame().scopes[i - 1];
            auto found = scope.locals.find(name);
            if (found) {
                return *found;
            }
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

bool Compiler::compile(node_ref_t root) {
    EASY_FUNCTION(::profiler::colors::Cyan200);

    prog.init();

    ir::ProgramBuilder builder{};
    builder.init(prog);

    CompileEngine engine{root, builder, err};
    engine.compile();

    auto str = prog.inspect();
    LOG_INF("ir:\n%.*s", str.size, str.data);

    return !engine.m_error_occurred;
}

} // namespace nkl
