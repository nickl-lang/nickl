#include "nkl/core/compiler.hpp"

namespace nkl {

namespace {

LOG_USE_SCOPE(nkl::compiler)

using namespace nk::vm::ir;

struct CompileEngine {
    node_ref_t m_root;
    ProgramBuilder &m_builder;
    string &m_err;

    bool m_error_occurred = false;

    void compile() {
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
