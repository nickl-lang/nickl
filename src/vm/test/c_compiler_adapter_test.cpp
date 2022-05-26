#include "nk/vm/c_compiler_adapter.hpp"

#include <gtest/gtest.h>

#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"
#include "nk/vm/vm.hpp"
#include "pipe_stream.hpp"
#include "utils/test_ir.hpp"

using namespace nk::vm;

namespace {

class c_compiler_adapter : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        VmConfig conf{};
        string paths[] = {cs2s(LIBS_SEARCH_PATH)};
        conf.find_library.search_paths = {paths, sizeof(paths) / sizeof(paths[0])};
        vm_init(conf);

        m_conf = {
            .compiler_binary = cs2s(TEST_C_COMPILER),
            .additional_flags = cs2s(TEST_C_COMPILER_FLAGS),
            .output_filename = tmpstr_format(
                TEST_FILES_DIR "%s_test.out",
                testing::UnitTest::GetInstance()->current_test_info()->name()),
            .quiet = TEST_QUIET,
        };

        m_prog.init();
        m_builder.init(m_prog);
    }

    void TearDown() override {
        m_prog.deinit();

        vm_deinit();
    }

protected:
    void _startMain() {
        auto i8_t = type_get_numeric(Int8);
        auto i32_t = type_get_numeric(Int32);
        auto argv_t = type_get_ptr(type_get_ptr(i8_t));

        type_t args[] = {i32_t, argv_t};
        auto args_t = type_get_tuple(TypeArray{args, sizeof(args) / sizeof(args[0])});

        m_builder.startFunct(m_builder.makeFunct(), cs2s("main"), i32_t, args_t);
        m_builder.startBlock(m_builder.makeLabel(), cs2s("start"));
    }

    ir::ExtFunctId _makePrintf() {
        auto i32_t = type_get_numeric(Int32);
        auto i8_ptr_t = type_get_ptr(type_get_numeric(Int8));

        type_t args[] = {i8_ptr_t};
        auto args_t = type_get_tuple({args, sizeof(args) / sizeof(args[0])});

        auto f_printf = m_builder.makeExtFunct(
            m_builder.makeShObj(cs2id(LIBC_NAME)), cs2id("printf"), i32_t, args_t, true);
        return f_printf;
    }

    std::string _runGetStdout() {
        auto in = pipe_streamRead(m_conf.output_filename);
        defer {
            pipe_streamClose(in);
        };

        std::string out_str{std::istreambuf_iterator<char>{in}, {}};
        return out_str;
    }

protected:
    CCompilerConfig m_conf;
    ir::Program m_prog;
    ir::ProgramBuilder m_builder;
};

} // namespace

TEST_F(c_compiler_adapter, empty) {
    auto src = c_compiler_streamOpen(m_conf);
    EXPECT_FALSE(c_compiler_streamClose(src));
}

TEST_F(c_compiler_adapter, empty_main) {
    auto src = c_compiler_streamOpen(m_conf);

    src << "int main() {}";

    EXPECT_TRUE(c_compiler_streamClose(src));
}

TEST_F(c_compiler_adapter, hello_world) {
    auto src = c_compiler_streamOpen(m_conf);

    src << "#include <stdio.h>\n"
           R"(int main() { printf("Hello, World!\n"); })";

    EXPECT_TRUE(c_compiler_streamClose(src));

    EXPECT_EQ(_runGetStdout(), "Hello, World!\n");
}

TEST_F(c_compiler_adapter, undefined_var) {
    auto src = c_compiler_streamOpen(m_conf);

    src << "#include <stdio.h>\n"
           R"(int main() { printf("%i", var); })";

    EXPECT_FALSE(c_compiler_streamClose(src));
}

TEST_F(c_compiler_adapter, compile_basic) {
    test_ir_main_argc(m_builder);

    EXPECT_TRUE(c_compiler_compile(m_conf, m_prog));
    EXPECT_EQ(_runGetStdout(), "");
}

TEST_F(c_compiler_adapter, pi) {
    test_ir_main_pi(m_builder, cs2s(LIBC_NAME));

    EXPECT_TRUE(c_compiler_compile(m_conf, m_prog));
    EXPECT_EQ(_runGetStdout(), "pi = 3.1415926535897940\n");
}

TEST_F(c_compiler_adapter, vec2LenSquared) {
    test_ir_main_vec2LenSquared(m_builder, cs2s(LIBC_NAME));

    EXPECT_TRUE(c_compiler_compile(m_conf, m_prog));
    EXPECT_EQ(_runGetStdout(), "lenSquared = 41.000000\n");
}
