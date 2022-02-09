#include "nk/vm/translate_to_c.hpp"

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/vm/bc.hpp"
#include "nk/vm/ir.hpp"
#include "nk/vm/vm.hpp"
#include "utils/test_ir.hpp"

using namespace nk::vm;

namespace {

LOG_USE_SCOPE(nk::vm::translate_to_c::test)

class translate_to_c : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        VmConfig conf{};
        string paths[] = {cs2s(LIBS_SEARCH_PATH)};
        conf.find_library.search_paths = {paths, sizeof(paths) / sizeof(paths[0])};
        vm_init(conf);

        m_arena.init();
        m_prog.init();
        m_builder.init(m_prog);
    }

    void TearDown() override {
        m_prog.deinit();
        m_arena.deinit();

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

protected:
    ArenaAllocator m_arena;
    ir::Program m_prog;
    ir::ProgramBuilder m_builder;
};

} // namespace

TEST_F(translate_to_c, basic) {
    test_ir_main(m_builder);

    auto str = m_prog.inspect();
    LOG_INF("ir:\n%.*s", str.size, str.data);

    translateToC(cs2s("test"), m_prog);
}

TEST_F(translate_to_c, pi) {
    test_ir_pi(m_builder);

    auto &b = m_builder;

    auto i8_t = type_get_numeric(Int8);
    auto i32_t = type_get_numeric(Int32);
    auto f64_t = type_get_numeric(Float64);

    auto i8_ptr_t = type_get_ptr(i8_t);

    auto f_printf = _makePrintf();
    auto f_pi = ir::FunctId{b.prog->functs[1].id};

    _startMain();

    type_t pf_args[] = {i8_ptr_t, f64_t};
    auto pf_args_t = type_get_tuple({pf_args, sizeof(pf_args) / sizeof(pf_args[0])});

    auto v_pf_args = b.makeFrameRef(b.makeLocalVar(pf_args_t));

    auto arg0 = v_pf_args.plus(type_tuple_offsetAt(pf_args_t, 0), i8_ptr_t);
    auto arg1 = v_pf_args.plus(type_tuple_offsetAt(pf_args_t, 1), f64_t);

    auto fmt = cs2s("pi = %.16lf\n");
    auto fmt_t = type_get_ptr(type_get_array(i8_t, fmt.size));

    b.gen(b.make_mov(arg0, b.makeConstRef(fmt.data, fmt_t)));
    b.gen(b.make_call(arg1, f_pi, {}));
    b.gen(b.make_call(b.makeRegRef(Reg_A, i32_t), f_printf, v_pf_args));

    b.gen(b.make_ret());

    auto str = m_prog.inspect();
    LOG_INF("ir:\n%.*s", str.size, str.data);

    translateToC(cs2s("test"), m_prog);
}

TEST_F(translate_to_c, vec2LenSquared) {
    test_ir_vec2LenSquared(m_builder);

    auto &b = m_builder;

    auto i8_t = type_get_numeric(Int8);
    auto i32_t = type_get_numeric(Int32);
    auto f64_t = type_get_numeric(Float64);

    type_t vec_types[] = {f64_t, f64_t};
    auto vec_t = type_get_tuple({vec_types, sizeof(vec_types) / sizeof(vec_types[0])});

    auto f_printf = _makePrintf();
    auto f_vec2LenSquared = ir::FunctId{b.prog->functs[0].id};

    auto vec_ptr_t = type_get_ptr(vec_t);
    auto f64_ptr_t = type_get_ptr(f64_t);

    auto i8_ptr_t = type_get_ptr(i8_t);

    _startMain();

    auto v_vec = b.makeFrameRef(b.makeLocalVar(vec_t));
    auto v_lenSquared = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_args = b.makeFrameRef(b.makeLocalVar(b.prog->functs[0].args_t));

    auto vec_arg0 = v_args.plus(type_tuple_offsetAt(vec_t, 0), vec_ptr_t);
    auto vec_arg1 = v_args.plus(type_tuple_offsetAt(vec_t, 1), f64_ptr_t);

    b.gen(b.make_mov(v_vec.plus(type_tuple_offsetAt(vec_t, 0), f64_t), b.makeConstRef(4.0, f64_t)));
    b.gen(b.make_mov(v_vec.plus(type_tuple_offsetAt(vec_t, 1), f64_t), b.makeConstRef(5.0, f64_t)));
    b.gen(b.make_lea(vec_arg0, v_vec));
    b.gen(b.make_lea(vec_arg1, v_lenSquared));
    b.gen(b.make_call({}, f_vec2LenSquared, v_args));

    type_t pf_args[] = {i8_ptr_t, f64_t};
    auto pf_args_t = type_get_tuple({pf_args, sizeof(pf_args) / sizeof(pf_args[0])});

    auto v_pf_args = b.makeFrameRef(b.makeLocalVar(pf_args_t));

    auto pf_arg0 = v_pf_args.plus(type_tuple_offsetAt(pf_args_t, 0), i8_ptr_t);
    auto pf_arg1 = v_pf_args.plus(type_tuple_offsetAt(pf_args_t, 1), f64_t);

    auto fmt = cs2s("lenSquared = %lf\n");
    auto fmt_t = type_get_ptr(type_get_array(i8_t, fmt.size));

    b.gen(b.make_mov(pf_arg0, b.makeConstRef(fmt.data, fmt_t)));
    b.gen(b.make_mov(pf_arg1, v_lenSquared));
    b.gen(b.make_call(b.makeRegRef(Reg_A, i32_t), f_printf, v_pf_args));

    b.gen(b.make_ret());

    auto str = m_prog.inspect();
    LOG_INF("ir:\n%.*s", str.size, str.data);

    translateToC(cs2s("test"), m_prog);
}
