#include "translate_to_c.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.hpp"
#include "nk/vm/bc.hpp"
#include "nk/vm/ir.hpp"
#include "nk/vm/vm.hpp"
#include "utils/test_ir.hpp"

using namespace nk::vm;

namespace {

class translate_to_c : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        VmConfig conf{};
        string paths[] = {cs2s(LIBS_SEARCH_PATH)};
        conf.find_library.search_paths = {paths, sizeof(paths) / sizeof(paths[0])};
        vm_init(conf);

        m_prog.init();
        m_builder.init(m_prog);
    }

    void TearDown() override {
        m_prog.deinit();

        vm_deinit();
    }

protected:
    ir::Program m_prog;
    ir::ProgramBuilder m_builder;
};

} // namespace

//@Incomplete Add expects to translateToC tests

TEST_F(translate_to_c, basic) {
    test_ir_main_argc(m_builder);

    translateToC(m_prog, std::cout);
}

TEST_F(translate_to_c, pi) {
    test_ir_main_pi(m_builder, cs2s(LIBC_NAME));

    translateToC(m_prog, std::cout);
}

TEST_F(translate_to_c, vec2LenSquared) {
    test_ir_main_vec2LenSquared(m_builder, cs2s(LIBC_NAME));

    translateToC(m_prog, std::cout);
}
