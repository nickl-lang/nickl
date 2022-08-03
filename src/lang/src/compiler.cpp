#include "nkl/lang/compiler.hpp"

#include "nk/ds/hashmap.hpp"
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

    Array<ScopeCtx> scopes{};
    bool m_is_top_level = true;

    ScopeCtx &curScope() const {
        assert(scopes.size && "no current scope");
        return scopes.back();
    }

    void pushScope() {
        LOG_DBG("entering scope=%lu", scopes.size);

        *scopes.push() = {};
    }

    void popScope() {
        LOG_DBG("exiting scope=%lu", scopes.size - 1);

        auto &scope = scopes.back();
        scope.locals.deinit();
        scopes.pop();
    }

    void compile() {
        defer {
            assert(!scopes.size && "unhandled scopes on the stack");
            scopes.deinit();
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

    Void compileNode(NodeRef node) {
        // CHECK(compileStmt(node));
        return {};
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

    CompileEngine engine{root, builder, err};
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
