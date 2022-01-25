#include "nk/vm/ir.hpp"

#include <gtest/gtest.h>

#include "nk/common/arena.hpp"
#include "nk/common/logger.hpp"
#include "nk/common/utils.hpp"

using namespace nk;
using namespace nk::vm::ir;

namespace {

class ir : public testing::Test {
    void SetUp() override {
        LOGGER_INIT(LoggerOptions{});

        m_arena.init();
        vm::types_init();

        m_builder.init();
    }

    void TearDown() override {
        m_builder.deinit();

        vm::types_deinit();
        m_arena.deinit();
    }

protected:
    ArenaAllocator m_arena;
    ProgramBuilder m_builder;
};

} // namespace

TEST_F(ir, basic) {
    using namespace nk::vm;

    auto ret_t = type_get_numeric(Int64);
    type_t args[] = {ret_t, ret_t};
    auto args_t = type_get_tuple(&m_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

    auto f = m_builder.makeFunct(cstr_to_str("plus"), ret_t, args_t);
    m_builder.startFunct(f);

    auto b = m_builder.makeLabel(cstr_to_str("start"));
    m_builder.startBlock(b);
    m_builder.comment(cstr_to_str("function entry point"));

    auto dst = m_builder.makeRetRef();
    auto lhs = m_builder.makeArgRef(0);
    auto rhs = m_builder.makeArgRef(1);

    m_builder.gen(m_builder.make_add(dst, lhs, rhs));
    m_builder.comment(cstr_to_str("add the arguments together"));

    m_builder.gen(m_builder.make_ret());
    m_builder.comment(cstr_to_str("return the result"));

    std::cout << m_builder.inspect(&m_arena).view();
}
