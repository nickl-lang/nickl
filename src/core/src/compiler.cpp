#include "nkl/core/compiler.hpp"

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

#define APPEND(AR, VAL)         \
    {                           \
        DEFINE(__val, VAL)      \
        AR.emplace_back(__val); \
    }

namespace nkl {

namespace {

LOG_USE_SCOPE(nkl::compiler)

using namespace nk::vm;
using namespace nk::vm::ir;

struct CompileEngine {
    node_ref_t m_root;
    ProgramBuilder &m_builder;
    string &m_err;

    bool m_error_occurred = false;

    enum EValueType : uint8_t {
        v_none = 0,

        v_instr,
        v_var,
        v_val,
    };

    struct ValueInfo {
        union Var {
            void *val;
            Instr instr;
            Local local;
        } as;
        type_t type;
        EValueType value_type;
    };

    void compile() {
        //@Todo push/pop frames

        compileScope(m_root);
    }

    int compileScope(node_ref_t node) {
        //@Todo push/pop scopes

        CHECK(compileStmt(node));

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

        default:
            assert(!"unreachable");
            break;
        }

        return {};
    }

    Ref makeRef(ValueInfo const &val) {
        switch (val.value_type) {
        case v_val:
            return m_builder.makeConstRef({val.as.val, val.type});

        case v_var:
            return m_builder.makeFrameRef(val.as.local);

        case v_instr: {
            auto instr = val.as.instr;
            auto &ref = instr.arg[0].as.ref;
            if (ref.ref_type == Ref_None && val.type->typeclass_id != Type_Void) {
                ref = m_builder.makeFrameRef(m_builder.makeLocalVar(val.type));
            }
            gen(instr);
            return ref;
        }

        default:
            return {};
        };
    }

    void gen(Instr const &instr) {
        EASY_FUNCTION(::profiler::colors::Cyan200)

        LOG_DBG("gen: %s", s_ir_names[instr.code])

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

    ProgramBuilder builder{};
    builder.init(prog);

    CompileEngine engine{root, builder, err};
    engine.compile();

    LOG_INF("code:\n", prog.inspect())

    return !engine.m_error_occurred;
}

} // namespace nkl
