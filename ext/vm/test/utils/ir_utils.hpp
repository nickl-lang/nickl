#ifndef HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS
#define HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS

#include "nk/vm/ir.hpp"

namespace nk {
namespace vm {
namespace ir {

inline void buildTestIr_plus(ProgramBuilder &builder) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i64_t = type_get_numeric(Int64);
    type_t args[] = {i64_t, i64_t};
    auto args_t = type_get_tuple(&tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

    auto f = builder.makeFunct();
    builder.startFunct(f, cstr_to_str("plus"), i64_t, args_t);

    auto b = builder.makeLabel();
    builder.startBlock(b, cstr_to_str("start"));
    builder.comment(cstr_to_str("function entry point"));

    auto dst = builder.makeRetRef();
    auto lhs = builder.makeArgRef(0);
    auto rhs = builder.makeArgRef(1);

    builder.gen(builder.make_add(dst, lhs, rhs));
    builder.comment(cstr_to_str("add the arguments together"));

    builder.gen(builder.make_ret());
    builder.comment(cstr_to_str("return the result"));
}

inline void buildTestIr_not(ProgramBuilder &builder) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i64_t = type_get_numeric(Int64);
    type_t args[] = {i64_t};
    auto args_t = type_get_tuple(&tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

    auto f = builder.makeFunct();
    builder.startFunct(f, cstr_to_str("not"), i64_t, args_t);

    auto arg = builder.makeArgRef(0);
    auto ret = builder.makeRetRef();

    int64_t true_val = 1;
    auto c_true = builder.makeConstRef({&true_val, i64_t});

    int64_t false_val = 0;
    auto c_false = builder.makeConstRef({&false_val, i64_t});

    auto l_else = builder.makeLabel();
    auto l_end = builder.makeLabel();
    auto l_start = builder.makeLabel();
    auto l_then = builder.makeLabel();

    builder.startBlock(l_start, cstr_to_str("start"));
    builder.gen(builder.make_jmpz(arg, l_else));

    builder.startBlock(l_then, cstr_to_str("then"));
    builder.gen(builder.make_mov(ret, c_false));
    builder.gen(builder.make_jmp(l_end));

    builder.startBlock(l_else, cstr_to_str("else"));
    builder.gen(builder.make_mov(ret, c_true));

    builder.startBlock(l_end, cstr_to_str("end"));
    builder.gen(builder.make_ret());
}

} // namespace ir
} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS
