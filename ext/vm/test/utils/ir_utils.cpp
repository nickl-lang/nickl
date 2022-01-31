#include "ir_utils.hpp"

namespace nk {
namespace vm {
namespace ir {

FunctId buildTestIr_plus(ProgramBuilder &b) {
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

FunctId buildTestIr_not(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i64_t = type_get_numeric(Int64);
    type_t args[] = {i64_t};
    auto args_t = type_get_tuple(&tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("not"), i64_t, args_t);

    auto arg = b.makeArgRef(0);
    auto ret = b.makeRetRef();

    auto c_true = b.makeConstRef(1l, i64_t);
    auto c_false = b.makeConstRef(0l, i64_t);

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

FunctId buildTestIr_atan(ProgramBuilder &b) {
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

    auto c_0u = b.makeConstRef(0ul, u64_t);
    auto c_1u = b.makeConstRef(1ul, u64_t);

    auto c_1f = b.makeConstRef(1.0, f64_t);
    auto c_n1f = b.makeConstRef(-1.0, f64_t);
    auto c_2f = b.makeConstRef(2.0, f64_t);

    auto l_start = b.makeLabel();
    auto l_loop = b.makeLabel();
    auto l_end = b.makeLabel();

    b.startBlock(l_start, cstr_to_str("start"));

    b.gen(b.make_mov(v_sum, a_x));
    b.gen(b.make_mov(v_sign, c_1f));
    b.gen(b.make_mov(v_num, a_x));
    b.gen(b.make_mov(v_denom, c_1f));
    b.gen(b.make_mov(v_i, c_0u));

    b.startBlock(l_loop, cstr_to_str("loop"));

    b.gen(b.make_lt(v_cond, v_i, a_it));
    b.gen(b.make_jmpz(v_cond, l_end));

    b.gen(b.make_mul(v_0, v_num, a_x));
    b.gen(b.make_mul(v_num, v_0, a_x));
    b.gen(b.make_add(v_denom, v_denom, c_2f));
    b.gen(b.make_mul(v_sign, v_sign, c_n1f));
    b.gen(b.make_mul(v_1, v_sign, v_num));
    b.gen(b.make_div(v_2, v_1, v_denom));
    b.gen(b.make_add(v_sum, v_sum, v_2));
    b.gen(b.make_add(v_i, v_i, c_1u));

    b.gen(b.make_jmp(l_loop));

    b.startBlock(l_end, cstr_to_str("end"));
    b.gen(b.make_mov(ret, v_sum));

    b.gen(b.make_ret());

    return f;
}

FunctId buildTestIr_pi(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto f_atan = buildTestIr_atan(b);

    auto u64_t = type_get_numeric(Uint64);
    auto f64_t = type_get_numeric(Float64);

    auto args_t = type_get_tuple(&tmp_arena, {});

    type_t atan_args[] = {f64_t, u64_t};
    auto atan_args_t =
        type_get_tuple(&tmp_arena, TypeArray{atan_args, sizeof(atan_args) / sizeof(atan_args[0])});

    struct AtanArgs {
        double x;
        uint64_t it;
    };

    AtanArgs a_args = {1.0 / 5.0, 10};
    AtanArgs b_args = {1.0 / 239.0, 10};

    auto c_a_args = b.makeConstRef({&a_args, atan_args_t});
    auto c_b_args = b.makeConstRef({&b_args, atan_args_t});

    auto ret = b.makeRetRef();

    auto c_4f = b.makeConstRef(4.0, f64_t);

    // TODO Hack with marking pi as entry point
    auto f = b.makeFunct(true);
    b.startFunct(f, cstr_to_str("pi"), f64_t, args_t);

    auto v_a = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_b = b.makeFrameRef(b.makeLocalVar(f64_t));

    auto l_start = b.makeLabel();

    b.startBlock(l_start, cstr_to_str("start"));

    b.gen(b.make_call(v_a, f_atan, c_a_args));
    b.gen(b.make_call(v_b, f_atan, c_b_args));
    b.gen(b.make_mul(v_a, v_a, c_4f));
    b.gen(b.make_sub(v_a, v_a, v_b));
    b.gen(b.make_mul(ret, v_a, c_4f));

    b.gen(b.make_ret());

    return f;
}

FunctId buildTestIr_rsqrt(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i32_t = type_get_numeric(Int32);
    auto f32_t = type_get_numeric(Float32);

    auto args_t = type_get_tuple(&tmp_arena, {&f32_t, 1});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("rsqrt"), f32_t, args_t);

    auto l_start = b.makeLabel();

    auto a_x = b.makeArgRef(0);

    auto c_0_5f = b.makeConstRef(0.5f, f32_t);
    auto c_1_5f = b.makeConstRef(1.5f, f32_t);
    auto c_1u = b.makeConstRef(1, i32_t);
    auto c_magic = b.makeConstRef(0x5f3759df, i32_t);

    b.startBlock(l_start, cstr_to_str("start"));

    // TODO Add support for reinterpret_cast in Ir construction
    auto v_0 = b.makeLocalVar(i32_t);
    auto v_0_i32 = b.makeFrameRef(v_0);
    v_0_i32.type = i32_t;
    auto v_0_f32 = b.makeFrameRef(v_0);
    v_0_f32.type = f32_t;

    auto v_1 = b.makeFrameRef(b.makeLocalVar(f32_t));

    auto ret_f32 = b.makeRetRef();
    auto ret_i32 = ret_f32;
    ret_i32.type = i32_t;

    b.gen(b.make_mov(ret_f32, a_x));
    b.gen(b.make_mul(v_1, a_x, c_0_5f));

    b.gen(b.make_rsh(v_0_i32, ret_i32, c_1u));
    b.gen(b.make_sub(ret_i32, c_magic, v_0_i32));

    b.gen(b.make_mul(v_0_f32, v_1, ret_f32));
    b.gen(b.make_mul(v_0_f32, v_0_f32, ret_f32));
    b.gen(b.make_sub(v_0_f32, c_1_5f, v_0_f32));
    b.gen(b.make_mul(ret_f32, ret_f32, v_0_f32));

    b.gen(b.make_mul(v_0_f32, v_1, ret_f32));
    b.gen(b.make_mul(v_0_f32, v_0_f32, ret_f32));
    b.gen(b.make_sub(v_0_f32, c_1_5f, v_0_f32));
    b.gen(b.make_mul(ret_f32, ret_f32, v_0_f32));

    b.gen(b.make_ret());

    return f;
}

FunctId buildTestIr_vec2LenSquared(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto void_t = type_get_void();
    auto f64_t = type_get_numeric(Float64);

    type_t vec_types[] = {f64_t, f64_t};
    auto vec_t = type_get_tuple(&tmp_arena, {vec_types, sizeof(vec_types) / sizeof(vec_types[0])});

    type_t vec_ptr_t = type_get_ptr(vec_t);
    type_t f64_ptr_t = type_get_ptr(f64_t);

    type_t args_types[] = {vec_ptr_t, f64_ptr_t};
    auto args_t =
        type_get_tuple(&tmp_arena, {args_types, sizeof(args_types) / sizeof(args_types[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("vec2LenSquared"), void_t, args_t);

    auto l_start = b.makeLabel();

    auto a_vec_ptr = b.makeArgRef(0);
    auto a_res_ptr = b.makeArgRef(1);

    auto v_0 = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_1 = b.makeFrameRef(b.makeLocalVar(f64_t));

    b.startBlock(l_start, cstr_to_str("start"));

    b.gen(b.make_mov(v_0, b.deref(a_vec_ptr, f64_t)));
    b.gen(b.make_mov(v_1, b.deref(a_vec_ptr, f64_t, 8)));
    b.gen(b.make_mul(v_0, v_0, v_0));
    b.gen(b.make_mul(v_1, v_1, v_1));
    b.gen(b.make_add(b.deref(a_res_ptr, f64_t), v_0, v_1));

    b.gen(b.make_ret());

    return f;
}

} // namespace ir
} // namespace vm
} // namespace nk
