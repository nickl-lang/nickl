#include "test_ir.hpp"

#include <iostream>

namespace nk {
namespace vm {
namespace ir {

void test_ir_plus(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i64_t = type_get_numeric(Int64);
    type_t args[] = {i64_t, i64_t};
    auto args_t = type_get_tuple(tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

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
}

void test_ir_not(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i64_t = type_get_numeric(Int64);
    type_t args[] = {i64_t};
    auto args_t = type_get_tuple(tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("not"), i64_t, args_t);

    auto arg = b.makeArgRef(0);
    auto ret = b.makeRetRef();

    auto c_true = b.makeConstRef(1ll, i64_t);
    auto c_false = b.makeConstRef(0ll, i64_t);

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
}

void test_ir_atan(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto u64_t = type_get_numeric(Uint64);
    auto f64_t = type_get_numeric(Float64);

    type_t args[] = {f64_t, u64_t};
    auto args_t = type_get_tuple(tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

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

    auto c_0u = b.makeConstRef(0ull, u64_t);
    auto c_1u = b.makeConstRef(1ull, u64_t);

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
}

void test_ir_pi(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    test_ir_atan(b);

    auto f64_t = type_get_numeric(Float64);

    auto args_t = type_get_tuple(tmp_arena, {});

    struct AtanArgs {
        double x;
        uint64_t it;
    };

    AtanArgs a_args = {1.0 / 5.0, 10};
    AtanArgs b_args = {1.0 / 239.0, 10};

    auto c_a_args = b.makeConstRef({&a_args, b.prog->functs[0].args_t});
    auto c_b_args = b.makeConstRef({&b_args, b.prog->functs[0].args_t});

    auto ret = b.makeRetRef();

    auto c_4f = b.makeConstRef(4.0, f64_t);

    //@Refactor/Feature Maybe add symbol resolution to VM programs?
    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("pi"), f64_t, args_t);

    auto v_a = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_b = b.makeFrameRef(b.makeLocalVar(f64_t));

    auto l_start = b.makeLabel();

    b.startBlock(l_start, cstr_to_str("start"));

    b.gen(b.make_call(v_a, FunctId{b.prog->functs[0].id}, c_a_args));
    b.gen(b.make_call(v_b, FunctId{b.prog->functs[0].id}, c_b_args));
    b.gen(b.make_mul(v_a, v_a, c_4f));
    b.gen(b.make_sub(v_a, v_a, v_b));
    b.gen(b.make_mul(ret, v_a, c_4f));

    b.gen(b.make_ret());
}

void test_ir_rsqrt(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i32_t = type_get_numeric(Int32);
    auto f32_t = type_get_numeric(Float32);

    auto args_t = type_get_tuple(tmp_arena, {&f32_t, 1});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("rsqrt"), f32_t, args_t);

    auto l_start = b.makeLabel();

    auto a_x = b.makeArgRef(0);

    auto c_0_5f = b.makeConstRef(0.5f, f32_t);
    auto c_1_5f = b.makeConstRef(1.5f, f32_t);
    auto c_1u = b.makeConstRef(1, i32_t);
    auto c_magic = b.makeConstRef(0x5f3759df, i32_t);

    b.startBlock(l_start, cstr_to_str("start"));

    auto v_0_i32 = b.makeFrameRef(b.makeLocalVar(i32_t));
    auto v_0_f32 = v_0_i32.as(f32_t);

    auto v_1 = b.makeFrameRef(b.makeLocalVar(f32_t));

    auto ret_f32 = b.makeRetRef();
    auto ret_i32 = ret_f32.as(i32_t);

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
}

void test_ir_vec2LenSquared(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto void_t = type_get_void();
    auto f64_t = type_get_numeric(Float64);

    type_t vec_types[] = {f64_t, f64_t};
    auto vec_t = type_get_tuple(tmp_arena, {vec_types, sizeof(vec_types) / sizeof(vec_types[0])});

    auto vec_ptr_t = type_get_ptr(vec_t);
    auto f64_ptr_t = type_get_ptr(f64_t);

    type_t args_ar[] = {vec_ptr_t, f64_ptr_t};
    auto args_t = type_get_tuple(tmp_arena, {args_ar, sizeof(args_ar) / sizeof(args_ar[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("vec2LenSquared"), void_t, args_t);

    auto l_start = b.makeLabel();

    auto a_vec_ptr = b.makeArgRef(0);
    auto a_res_ptr = b.makeArgRef(1);

    auto r_a = b.makeRegRef(Reg_A, f64_t);
    auto r_b = b.makeRegRef(Reg_B, f64_t);

    b.startBlock(l_start, cstr_to_str("start"));

    b.gen(b.make_mov(r_a, a_vec_ptr.deref(f64_t)));
    b.gen(b.make_mov(r_b, a_vec_ptr.deref(f64_t).plus(8)));
    b.gen(b.make_mul(r_a, r_a, r_a));
    b.gen(b.make_mul(r_b, r_b, r_b));
    b.gen(b.make_add(a_res_ptr.deref(), r_a, r_b));

    b.gen(b.make_ret());
}

void test_ir_modf(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i64_t = type_get_numeric(Int64);
    auto f64_t = type_get_numeric(Float64);
    auto f64_ptr_t = type_get_ptr(f64_t);
    auto typeref_t = type_get_typeref();

    type_t args_ar[] = {f64_t, f64_ptr_t};
    auto args_t = type_get_tuple(tmp_arena, {args_ar, sizeof(args_ar) / sizeof(args_ar[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("modf"), f64_t, args_t);

    auto a_x = b.makeArgRef(0);
    auto a_int_part_ptr = b.makeArgRef(1);

    auto ret = b.makeRetRef();

    auto v_int_part = b.makeFrameRef(b.makeLocalVar(i64_t));

    auto l_start = b.makeLabel();

    b.startBlock(l_start, cstr_to_str("start"));

    auto int_part_ref = a_int_part_ptr.deref();

    b.gen(b.make_cast(v_int_part, b.makeConstRef(i64_t, typeref_t), a_x));
    b.gen(b.make_cast(int_part_ref, b.makeConstRef(f64_t, typeref_t), v_int_part));
    b.gen(b.make_sub(ret, a_x, int_part_ref));

    b.gen(b.make_ret());
}

void test_ir_intPart(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    test_ir_modf(b);
    auto f_modf_id = b.prog->functs[0].id;

    auto f64_t = type_get_numeric(Float64);

    auto args_t = type_get_tuple(tmp_arena, {&f64_t, 1});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("intPart"), f64_t, args_t);

    auto modf_args_t = b.prog->functs[f_modf_id].args_t;

    auto a_x = b.makeArgRef(0);

    auto ret = b.makeRetRef();

    auto v_0 = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_args = b.makeFrameRef(b.makeLocalVar(modf_args_t));

    auto v_arg0 =
        v_args.plus(type_tuple_offset(modf_args_t, 0), modf_args_t->as.tuple.elems[0].type);
    auto v_arg1 =
        v_args.plus(type_tuple_offset(modf_args_t, 1), modf_args_t->as.tuple.elems[1].type);

    auto l_start = b.makeLabel();

    b.startBlock(l_start, cstr_to_str("start"));

    b.gen(b.make_mov(v_arg0, a_x));
    b.gen(b.make_lea(v_arg1, ret));
    b.gen(b.make_call(v_0, FunctId{f_modf_id}, v_args));

    b.gen(b.make_ret());
}

void test_ir_call10Times(ProgramBuilder &b, type_t fn) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto void_t = type_get_void();
    auto args_t = type_get_tuple(tmp_arena, {});

    auto i64_t = type_get_numeric(Int64);

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("call10Times"), void_t, args_t);

    auto v_i = b.makeFrameRef(b.makeLocalVar(i64_t));
    auto v_cond = b.makeFrameRef(b.makeLocalVar(i64_t));

    auto l_start = b.makeLabel();
    auto l_loop = b.makeLabel();
    auto l_end = b.makeLabel();

    b.startBlock(l_start, cstr_to_str("start"));

    b.gen(b.make_mov(v_i, b.makeConstRef(10ll, i64_t)));

    b.startBlock(l_loop, cstr_to_str("loop"));

    b.gen(b.make_gt(v_cond, v_i, b.makeConstRef(0ll, i64_t)));
    b.gen(b.make_jmpz(v_cond, l_end));

    b.gen(b.make_call({}, b.makeConstRef({nullptr, fn}), {}));
    b.gen(b.make_sub(v_i, v_i, b.makeConstRef(1ll, i64_t)));
    b.gen(b.make_jmp(l_loop));

    b.startBlock(l_end, cstr_to_str("end"));

    b.gen(b.make_ret());
}

void test_ir_hasZeroByte32(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i32_t = type_get_numeric(Int32);

    type_t args_ar[] = {i32_t};
    auto args_t = type_get_tuple(tmp_arena, {args_ar, sizeof(args_ar) / sizeof(args_ar[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("hasZeroByte32"), i32_t, args_t);

    auto a_x = b.makeArgRef(0);

    auto ret = b.makeRetRef();

    auto r_a = b.makeRegRef(Reg_A, i32_t);

    auto l_start = b.makeLabel();

    b.startBlock(l_start, cstr_to_str("start"));

    b.gen(b.make_sub(ret, a_x, b.makeConstRef(0x01010101u, i32_t)));
    b.gen(b.make_compl(r_a, a_x));
    b.gen(b.make_bitand(ret, ret, r_a));
    b.gen(b.make_bitand(ret, ret, b.makeConstRef(0x80808080u, i32_t)));

    b.gen(b.make_ret());
}

void test_ir_readToggleGlobal(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto void_t = type_get_void();
    auto args_t = type_get_tuple(tmp_arena, {});

    auto i64_t = type_get_numeric(Int64);

    auto g_var = b.makeGlobalRef(b.makeGlobalVar(i64_t));

    {
        auto f = b.makeFunct();
        b.startFunct(f, cstr_to_str("readGlobal"), i64_t, args_t);

        auto ret = b.makeRetRef();

        b.startBlock(b.makeLabel(), cstr_to_str("start"));
        b.gen(b.make_mov(ret, g_var));
        b.gen(b.make_ret());
    }

    {
        auto f = b.makeFunct();
        b.startFunct(f, cstr_to_str("toggleGlobal"), void_t, args_t);

        b.startBlock(b.makeLabel(), cstr_to_str("start"));
        b.gen(b.make_not(g_var, g_var));
        b.gen(b.make_ret());
    }
}

void test_ir_callNativeFunc(ProgramBuilder &b, void *fn_ptr) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto void_t = type_get_void();
    auto args_t = type_get_tuple(tmp_arena, {});

    auto fn_t = type_get_fn_native(void_t, args_t, 0, fn_ptr, nullptr);

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("callNativeFunc"), void_t, args_t);

    b.startBlock(b.makeLabel(), cstr_to_str("start"));

    b.gen(b.make_call({}, b.makeConstRef({nullptr, fn_t}), {}));

    b.gen(b.make_ret());
}

void test_ir_callNativeAdd(ProgramBuilder &b, void *fn_ptr) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i64_t = type_get_numeric(Int64);

    type_t args_ar[] = {i64_t, i64_t};
    auto args_t = type_get_tuple(tmp_arena, {args_ar, sizeof(args_ar) / sizeof(args_ar[0])});

    auto fn_t = type_get_fn_native(i64_t, args_t, 0, fn_ptr, nullptr);

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("callNativeAdd"), i64_t, args_t);

    auto ret = b.makeRetRef();

    b.startBlock(b.makeLabel(), cstr_to_str("start"));

    b.gen(b.make_call(ret, b.makeConstRef({nullptr, fn_t}), b.makeArgRef(0).as(args_t)));

    b.gen(b.make_ret());
}

void test_ir_callExternalPrintf(ProgramBuilder &b, string libname) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto void_t = type_get_void();
    auto u8_t = type_get_numeric(Uint8);
    auto i32_t = type_get_numeric(Int32);
    auto i64_t = type_get_numeric(Int64);

    auto u8_ptr_t = type_get_ptr(u8_t);

    auto args_t = type_get_tuple(tmp_arena, {});

    type_t printf_args[] = {u8_ptr_t, i64_t, i64_t, i64_t};
    auto printf_args_t =
        type_get_tuple(tmp_arena, {printf_args, sizeof(printf_args) / sizeof(printf_args[0])});

    auto f_printf = b.makeExtFunct(
        b.makeShObj(str_to_id(libname)), cstr_to_id("test_printf"), i32_t, printf_args_t);

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("callExternalPrintf"), void_t, args_t);

    auto args = b.makeFrameRef(b.makeLocalVar(printf_args_t));

    auto arg0 = args.plus(type_tuple_offset(printf_args_t, 0), u8_ptr_t);
    auto arg1 = args.plus(type_tuple_offset(printf_args_t, 1), i64_t);
    auto arg2 = args.plus(type_tuple_offset(printf_args_t, 2), i64_t);
    auto arg3 = args.plus(type_tuple_offset(printf_args_t, 3), i64_t);

    b.startBlock(b.makeLabel(), cstr_to_str("start"));

    b.gen(b.make_mov(arg0, b.makeConstRef((char const *)"%lli + %lli = %lli\n", u8_ptr_t)));
    b.gen(b.make_mov(arg1, b.makeConstRef(4ll, i64_t)));
    b.gen(b.make_mov(arg2, b.makeConstRef(5ll, i64_t)));
    b.gen(b.make_add(arg3, arg1, arg2));
    b.gen(b.make_call(b.makeRegRef(Reg_A, i32_t), f_printf, args));

    b.gen(b.make_ret());
}

void test_ir_getSetExternalVar(ProgramBuilder &b, string libname) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto void_t = type_get_void();
    auto i64_t = type_get_numeric(Int64);

    auto args_t = type_get_tuple(tmp_arena, {});

    auto v_test_val = b.makeExtVarRef(
        b.makeExtVar(b.makeShObj(str_to_id(libname)), cstr_to_id("test_val"), i64_t));

    b.startFunct(b.makeFunct(), cstr_to_str("getExternalVar"), i64_t, args_t);

    auto ret = b.makeRetRef();

    b.startBlock(b.makeLabel(), cstr_to_str("start"));
    b.gen(b.make_mov(ret, v_test_val));
    b.gen(b.make_ret());

    b.startFunct(b.makeFunct(), cstr_to_str("setExternalVar"), void_t, args_t);

    b.startBlock(b.makeLabel(), cstr_to_str("start"));
    b.gen(b.make_mov(v_test_val, b.makeConstRef(42ll, i64_t)));
    b.gen(b.make_ret());
}

void test_ir_main(ProgramBuilder &b) {
    auto tmp_arena = ArenaAllocator::create();
    DEFER({ tmp_arena.deinit(); })

    auto i8_t = type_get_numeric(Int8);
    auto i32_t = type_get_numeric(Int32);
    auto argv_t = type_get_ptr(type_get_ptr(i8_t));

    type_t args[] = {i32_t, argv_t};
    auto args_t = type_get_tuple(tmp_arena, TypeArray{args, sizeof(args) / sizeof(args[0])});

    auto f = b.makeFunct();
    b.startFunct(f, cstr_to_str("main"), i32_t, args_t);

    auto ret = b.makeRetRef();

    auto a_argc = b.makeArgRef(0);
    // auto a_argv = b.makeArgRef(1);

    auto l_start = b.makeLabel();
    auto l_error = b.makeLabel();

    b.startBlock(l_start, cstr_to_str("start"));
    b.gen(b.make_eq(b.makeRegRef(Reg_A, i32_t), a_argc, b.makeConstRef(3, i32_t)));
    b.gen(b.make_jmpz(b.makeRegRef(Reg_A, i32_t), l_error));
    b.gen(b.make_mov(ret, b.makeConstRef(0, i32_t)));
    b.gen(b.make_ret());

    b.startBlock(l_error, cstr_to_str("error"));
    b.gen(b.make_mov(ret, b.makeConstRef(1, i32_t)));
    b.gen(b.make_ret());
}

} // namespace ir
} // namespace vm
} // namespace nk
