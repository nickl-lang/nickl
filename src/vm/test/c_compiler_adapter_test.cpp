#include "c_compiler_adapter.hpp"

#include <iterator>

#include <gtest/gtest.h>

#include "nk/str/dynamic_string_builder.hpp"
#include "nk/str/static_string_builder.hpp"
#include "nk/utils/logger.h"
#include "nk/utils/utils.hpp"
#include "nk/vm/ir.hpp"
#include "pipe_stream.hpp"
#include "utils/test_ir.hpp"

namespace {

using namespace nk::vm;
using namespace nk;

LOG_USE_SCOPE(nk::vm::test::c_compiler_adapter);

class c_compiler_adapter : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        id_init();
        types::init();

        StaticStringBuilder{m_output_filename}.printf(
            TEST_FILES_DIR "%s_test.out",
            testing::UnitTest::GetInstance()->current_test_info()->name());

        m_conf = {
            .compiler_binary = cs2s(TEST_C_COMPILER),
            .additional_flags = cs2s(TEST_C_COMPILER_FLAGS),
            .output_filename = m_output_filename,
            .quiet = TEST_QUIET,
        };
    }

    void TearDown() override {
        m_arena.deinit();
        m_sb.deinit();

        m_prog.deinit();

        types::deinit();
        id_deinit();
    }

protected:
    std::string _runGetStdout() {
        auto in = pipe_streamRead(m_conf.output_filename);
        defer {
            pipe_streamClose(in);
        };

        std::string out_str{std::istreambuf_iterator<char>{in}, {}};
        LOG_DBG("out_str=%s", out_str.c_str());
        return out_str;
    }

protected:
    ARRAY_SLICE(char, m_output_filename, 1024);
    CCompilerConfig m_conf;

    ir::Program m_prog{};
    ir::ProgramBuilder m_builder{m_prog};

    DynamicStringBuilder m_sb{};
    StackAllocator m_arena{};
};

} // namespace

TEST_F(c_compiler_adapter, empty) {
    auto src = c_compiler_streamOpen(m_conf);
    EXPECT_FALSE(c_compiler_streamClose(src));
}

TEST_F(c_compiler_adapter, empty_main) {
    auto src = c_compiler_streamOpen(m_conf);

    src << "int main() {}\n";

    EXPECT_TRUE(c_compiler_streamClose(src));
}

TEST_F(c_compiler_adapter, hello_world) {
    auto src = c_compiler_streamOpen(m_conf);

    src << R"(
#include <stdio.h>
int main() {
    printf("Hello, World!");
}
)";

    EXPECT_TRUE(c_compiler_streamClose(src));

    EXPECT_EQ(_runGetStdout(), "Hello, World!");
}

TEST_F(c_compiler_adapter, undefined_var) {
    auto src = c_compiler_streamOpen(m_conf);

    src << R"(
#include <stdio.h>"
int main() {
    printf("%i", var);
}
)";

    EXPECT_FALSE(c_compiler_streamClose(src));
}

TEST_F(c_compiler_adapter, compile_basic) {
    test_ir_main_argc(m_builder);
    LOG_INF("ir:\n%s", [&]() {
        return m_prog.inspect(m_sb).moveStr(m_arena).data;
    }());

    ASSERT_TRUE(c_compiler_compile(m_conf, m_prog));
    EXPECT_EQ(_runGetStdout(), "");
}

TEST_F(c_compiler_adapter, pi) {
    test_ir_main_pi(m_builder, cs2s(LIBC_NAME));
    LOG_INF("ir:\n%s", [&]() {
        return m_prog.inspect(m_sb).moveStr(m_arena).data;
    }());

    ASSERT_TRUE(c_compiler_compile(m_conf, m_prog));
    EXPECT_EQ(_runGetStdout(), "pi = 3.1415926535897940\n");
}

TEST_F(c_compiler_adapter, vec2LenSquared) {
    test_ir_main_vec2LenSquared(m_builder, cs2s(LIBC_NAME));
    LOG_INF("ir:\n%s", [&]() {
        return m_prog.inspect(m_sb).moveStr(m_arena).data;
    }());

    ASSERT_TRUE(c_compiler_compile(m_conf, m_prog));
    EXPECT_EQ(_runGetStdout(), "lenSquared = 41.000000\n");
}
