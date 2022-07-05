#include "translate_to_c.hpp"

#include <gtest/gtest.h>

#include "find_library.hpp"
#include "nk/utils/logger.h"
#include "nk/vm/bc.hpp"
#include "nk/vm/ir.hpp"
#include "utils/test_ir.hpp"

namespace {

using namespace nk::vm;
using namespace nk;

class translate_to_c : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        FindLibraryConfig conf{};
        ARRAY_SLICE_INIT(string, paths, cs2s(LIBS_SEARCH_PATH));
        conf.search_paths = paths;
        findLibrary_init(conf);

        m_prog.init();
    }

    void TearDown() override {
        m_prog.deinit();

        findLibrary_deinit();
    }

protected:
    ir::Program m_prog;
    ir::ProgramBuilder m_builder{m_prog};
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
