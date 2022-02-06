#include "nk/vm/c_compiler.hpp"

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/vm/ir.hpp"
#include "utils/test_ir.hpp"

using namespace nk::vm;

namespace {

LOG_USE_SCOPE(nk::vm::c_compiler::test)

class c_compiler : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        id_init();

        m_arena.init();
        types_init();

        m_prog.init();
        m_builder.init(m_prog);
    }

    void TearDown() override {
        m_prog.deinit();

        types_deinit();
        m_arena.deinit();

        id_deinit();
    }

protected:
    ArenaAllocator m_arena;
    ir::Program m_prog;
    ir::ProgramBuilder m_builder;

    CCompiler m_compiler;
};

} // namespace

static void _buildMain(ir::ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i8_t = type_get_numeric(Int8);
    auto i32_t = type_get_numeric(Int32);
    auto argv_t = type_get_ptr(type_get_ptr(i8_t));

    type_t args[] = {i32_t, argv_t};
    auto args_t = type_get_tuple(tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("main"), i32_t, args_t);
    b.startBlock(b.makeLabel(), cstr_to_str("start"));
}

TEST_F(c_compiler, basic) {
    test_ir_main(m_builder);

    auto str = m_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    m_compiler.compile(cstr_to_str("test"), m_prog);
}

TEST_F(c_compiler, pi) {
    test_ir_pi(m_builder);

    auto &b = m_builder;

    auto u8_t = type_get_numeric(Uint8);
    auto i32_t = type_get_numeric(Int32);
    auto f64_t = type_get_numeric(Float64);

    auto u8_ptr_t = type_get_ptr(u8_t);

    type_t printf_args[] = {u8_ptr_t, f64_t};
    auto printf_args_t =
        type_get_tuple(m_arena, {printf_args, sizeof(printf_args) / sizeof(printf_args[0])});

    auto f_printf = b.makeExtFunct(
        b.makeShObj(cstr_to_id("libc.so")), cstr_to_id("printf"), i32_t, printf_args_t);

    _buildMain(m_builder);

    auto args = b.makeFrameRef(b.makeLocalVar(printf_args_t));

    auto arg0 = args.plus(type_tuple_offset(printf_args_t, 0), u8_ptr_t);
    auto arg1 = args.plus(type_tuple_offset(printf_args_t, 1), f64_t);

    auto fmt = cstr_to_str("pi = %.16lf\n");
    auto fmt_t = type_get_ptr(type_get_array(u8_t, fmt.size));

    b.gen(b.make_mov(arg0, b.makeConstRef(fmt.data, fmt_t)));
    b.gen(b.make_call(arg1, ir::FunctId{b.prog->functs[1].id}, {}));
    b.gen(b.make_call(b.makeRegRef(Reg_A, i32_t), f_printf, args));

    b.gen(b.make_ret());

    auto str = m_prog.inspect(m_arena);
    LOG_INF("ir:\n%.*s", str.size, str.data);

    m_compiler.compile(cstr_to_str("test"), m_prog);
}
