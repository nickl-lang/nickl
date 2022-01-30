#ifndef HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS
#define HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS

#include "nk/vm/ir.hpp"

namespace nk {
namespace vm {
namespace ir {

inline FunctId buildTestIr_plus(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i64_t = type_get_numeric(Int64);
    type_t args[] = {i64_t, i64_t};
    auto args_t = type_get_tuple(&tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("plus"), i64_t, args_t);

    b.startBlock(b.makeLabel(), cstr_to_str("start"));
    b.comment(cstr_to_str("function entry point"));

    auto dst = b.makeRetRef();
    auto lhs = b.makeArgRef(0);
    auto rhs = b.makeArgRef(1);

    b.gen(b.make_add(dst, lhs, rhs));
    b.comment(cstr_to_str("add the arguments together"));

    b.gen(b.make_ret());
    b.comment(cstr_to_str("return the result"));

    return f;
}

inline FunctId buildTestIr_not(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i64_t = type_get_numeric(Int64);
    type_t args[] = {i64_t};
    auto args_t = type_get_tuple(&tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("not"), i64_t, args_t);

    auto arg = b.makeArgRef(0);
    auto ret = b.makeRetRef();

    int64_t true_val = 1;
    auto c_true = b.makeConstRef({&true_val, i64_t});

    int64_t false_val = 0;
    auto c_false = b.makeConstRef({&false_val, i64_t});

    auto l_else = b.makeLabel();
    auto l_end = b.makeLabel();
    auto l_start = b.makeLabel();
    auto l_then = b.makeLabel();

    b.startBlock(l_start, cstr_to_str("start"));
    b.gen(b.make_jmpz(arg, l_else));

    b.startBlock(l_then, cstr_to_str("then"));
    b.gen(b.make_mov(ret, c_false));
    b.gen(b.make_jmp(l_end));

    b.startBlock(l_else, cstr_to_str("else"));
    b.gen(b.make_mov(ret, c_true));

    b.startBlock(l_end, cstr_to_str("end"));
    b.gen(b.make_ret());

    return f;
}

inline FunctId buildTestIr_atan(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto u64_t = type_get_numeric(Uint64);
    auto f64_t = type_get_numeric(Float64);

    type_t args[] = {f64_t, u64_t};
    auto args_t = type_get_tuple(&tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("atan"), f64_t, args_t);

    auto a_x = b.makeArgRef(0);
    auto a_it = b.makeArgRef(1);

    auto ret = b.makeRetRef();

    auto v_sum = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_sign = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_num = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_denom = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_i = b.makeFrameRef(b.makeLocalVar(u64_t));

    auto v_0 = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_1 = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_2 = b.makeFrameRef(b.makeLocalVar(f64_t));

    auto v_cond = b.makeFrameRef(b.makeLocalVar(u64_t));

    uint64_t _u_val = 0;
    double _f_val = 0;

    _u_val = 0;
    auto c_u0 = b.makeConstRef({&_u_val, u64_t});
    _u_val = 1;
    auto c_u1 = b.makeConstRef({&_u_val, u64_t});

    _f_val = 1.0;
    auto c_f1 = b.makeConstRef({&_f_val, f64_t});
    _f_val = -1.0;
    auto c_fn1 = b.makeConstRef({&_f_val, f64_t});
    _f_val = 2.0;
    auto c_f2 = b.makeConstRef({&_f_val, f64_t});

    auto l_start = b.makeLabel();
    auto l_loop = b.makeLabel();
    auto l_end = b.makeLabel();

    b.startBlock(l_start, cstr_to_str("start"));

    b.gen(b.make_mov(v_sum, a_x));
    b.gen(b.make_mov(v_sign, c_f1));
    b.gen(b.make_mov(v_num, a_x));
    b.gen(b.make_mov(v_denom, c_f1));
    b.gen(b.make_mov(v_i, c_u0));

    b.startBlock(l_loop, cstr_to_str("loop"));

    b.gen(b.make_lt(v_cond, v_i, a_it));
    b.gen(b.make_jmpz(v_cond, l_end));

    b.gen(b.make_mul(v_0, v_num, a_x));
    b.gen(b.make_mul(v_num, v_0, a_x));
    b.gen(b.make_add(v_denom, v_denom, c_f2));
    b.gen(b.make_mul(v_sign, v_sign, c_fn1));
    b.gen(b.make_mul(v_1, v_sign, v_num));
    b.gen(b.make_div(v_2, v_1, v_denom));
    b.gen(b.make_add(v_sum, v_sum, v_2));
    b.gen(b.make_add(v_i, v_i, c_u1));

    b.gen(b.make_jmp(l_loop));

    b.startBlock(l_end, cstr_to_str("end"));
    b.gen(b.make_mov(ret, v_sum));

    b.gen(b.make_ret());

    return f;
}

} // namespace ir
} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_TEST_UTILS_IR_UTILS
