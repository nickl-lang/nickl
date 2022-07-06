#include "test_ir.hpp"

#include <iostream>

#include "nk/utils/profiler.hpp"

namespace nk {
namespace vm {

namespace {

using namespace ir;

void _startMain(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto i8_t = type_get_numeric(Int8);
    auto i32_t = type_get_numeric(Int32);
    auto argv_t = type_get_ptr(type_get_ptr(i8_t));

    ARRAY_SLICE_INIT(type_t, args, i32_t, argv_t);
    auto args_t = type_get_tuple(args);

    b.startFunct(b.makeFunct(), cs2s("main"), i32_t, args_t);
    b.startBlock(b.makeLabel(), cs2s("start"));
}

ir::ExtFunctId _makePrintf(ProgramBuilder &b, string libc_name) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto i32_t = type_get_numeric(Int32);
    auto i8_ptr_t = type_get_ptr(type_get_numeric(Int8));

    ARRAY_SLICE_INIT(type_t, args, i8_ptr_t);
    auto args_t = type_get_tuple(args);

    auto f_printf =
        b.makeExtFunct(b.makeShObj(s2id(libc_name)), cs2id("printf"), i32_t, args_t, true);
    return f_printf;
}

} // namespace

ir::FunctId test_ir_plus(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto i64_t = type_get_numeric(Int64);

    ARRAY_SLICE_INIT(type_t, args, i64_t, i64_t);
    auto args_t = type_get_tuple(args);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("plus"), i64_t, args_t);

    b.startBlock(b.makeLabel(), cs2s("start"));

    auto dst = b.makeRetRef();
    auto lhs = b.makeArgRef(0);
    auto rhs = b.makeArgRef(1);

    b.gen(b.make_add(dst, lhs, rhs));

    b.gen(b.make_ret());

    return funct;
}

ir::FunctId test_ir_not(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto i64_t = type_get_numeric(Int64);

    ARRAY_SLICE_INIT(type_t, args, i64_t);
    auto args_t = type_get_tuple(args);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("not"), i64_t, args_t);

    auto arg = b.makeArgRef(0);
    auto ret = b.makeRetRef();

    auto c_true = b.makeConstRef(1ll, i64_t);
    auto c_false = b.makeConstRef(0ll, i64_t);

    auto l_else = b.makeLabel();
    auto l_end = b.makeLabel();
    auto l_start = b.makeLabel();
    auto l_then = b.makeLabel();

    b.startBlock(l_start, cs2s("start"));
    b.gen(b.make_jmpz(arg, l_else));

    b.startBlock(l_then, cs2s("then"));
    b.gen(b.make_mov(ret, c_false));
    b.gen(b.make_jmp(l_end));

    b.startBlock(l_else, cs2s("else"));
    b.gen(b.make_mov(ret, c_true));

    b.startBlock(l_end, cs2s("end"));
    b.gen(b.make_ret());

    return funct;
}

ir::FunctId test_ir_atan(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto u64_t = type_get_numeric(Uint64);
    auto f64_t = type_get_numeric(Float64);

    ARRAY_SLICE_INIT(type_t, args, f64_t, u64_t);
    auto args_t = type_get_tuple(args);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("atan"), f64_t, args_t);

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

    b.startBlock(l_start, cs2s("start"));

    b.gen(b.make_mov(v_sum, a_x));
    b.gen(b.make_mov(v_sign, c_1f));
    b.gen(b.make_mov(v_num, a_x));
    b.gen(b.make_mov(v_denom, c_1f));
    b.gen(b.make_mov(v_i, c_0u));

    b.startBlock(l_loop, cs2s("loop"));
    b.gen(b.make_enter());

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

    b.gen(b.make_leave());
    b.gen(b.make_jmp(l_loop));

    b.startBlock(l_end, cs2s("end"));
    b.gen(b.make_mov(ret, v_sum));

    b.gen(b.make_ret());

    return funct;
}

ir::FunctId test_ir_pi(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto const atan_funct = test_ir_atan(b);

    auto f64_t = type_get_numeric(Float64);

    auto args_t = type_get_tuple({});

    struct AtanArgs {
        double x;
        uint64_t it;
    };

    AtanArgs a_args = {1.0 / 5.0, 10};
    AtanArgs b_args = {1.0 / 239.0, 10};

    //@Robustness Referencing `b.prog.functs` directly
    auto c_a_args = b.makeConstRef({&a_args, b.prog.functs[0].args_t});
    auto c_b_args = b.makeConstRef({&b_args, b.prog.functs[0].args_t});

    auto ret = b.makeRetRef();

    auto c_4f = b.makeConstRef(4.0, f64_t);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("pi"), f64_t, args_t);

    auto v_a = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_b = b.makeFrameRef(b.makeLocalVar(f64_t));

    auto l_start = b.makeLabel();

    b.startBlock(l_start, cs2s("start"));

    b.gen(b.make_call(v_a, atan_funct, c_a_args));
    b.gen(b.make_call(v_b, atan_funct, c_b_args));
    b.gen(b.make_mul(v_a, v_a, c_4f));
    b.gen(b.make_sub(v_a, v_a, v_b));
    b.gen(b.make_mul(ret, v_a, c_4f));

    b.gen(b.make_ret());

    return funct;
}

ir::FunctId test_ir_rsqrt(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto i32_t = type_get_numeric(Int32);
    auto f32_t = type_get_numeric(Float32);

    auto args_t = type_get_tuple({&f32_t, 1});

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("rsqrt"), f32_t, args_t);

    auto l_start = b.makeLabel();

    auto a_x = b.makeArgRef(0);

    auto c_0_5f = b.makeConstRef(0.5f, f32_t);
    auto c_1_5f = b.makeConstRef(1.5f, f32_t);
    auto c_1u = b.makeConstRef(1, i32_t);
    auto c_magic = b.makeConstRef(0x5f3759df, i32_t);

    b.startBlock(l_start, cs2s("start"));

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

    return funct;
}

ir::FunctId test_ir_vec2LenSquared(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto void_t = type_get_void();
    auto f64_t = type_get_numeric(Float64);

    ARRAY_SLICE_INIT(type_t, vec_types, f64_t, f64_t);
    auto vec_t = type_get_tuple(vec_types);

    auto vec_ptr_t = type_get_ptr(vec_t);
    auto f64_ptr_t = type_get_ptr(f64_t);

    ARRAY_SLICE_INIT(type_t, args, vec_ptr_t, f64_ptr_t);
    auto args_t = type_get_tuple(args);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("vec2LenSquared"), void_t, args_t);

    auto l_start = b.makeLabel();

    auto a_vec_ptr = b.makeArgRef(0);
    auto a_res_ptr = b.makeArgRef(1);

    auto r_a = b.makeRegRef(Reg_A, f64_t);
    auto r_b = b.makeRegRef(Reg_B, f64_t);

    b.startBlock(l_start, cs2s("start"));

    b.gen(b.make_mov(r_a, a_vec_ptr.deref(f64_t)));
    b.gen(b.make_mov(r_b, a_vec_ptr.deref(f64_t).plus(8)));
    b.gen(b.make_mul(r_a, r_a, r_a));
    b.gen(b.make_mul(r_b, r_b, r_b));
    b.gen(b.make_add(a_res_ptr.deref(), r_a, r_b));

    b.gen(b.make_ret());

    return funct;
}

ir::FunctId test_ir_modf(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto i64_t = type_get_numeric(Int64);
    auto f64_t = type_get_numeric(Float64);
    auto f64_ptr_t = type_get_ptr(f64_t);
    auto void_ptr_t = type_get_ptr(type_get_void());

    ARRAY_SLICE_INIT(type_t, args, f64_t, f64_ptr_t);
    auto args_t = type_get_tuple(args);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("modf"), f64_t, args_t);

    auto a_x = b.makeArgRef(0);
    auto a_int_part_ptr = b.makeArgRef(1);

    auto ret = b.makeRetRef();

    auto v_int_part = b.makeFrameRef(b.makeLocalVar(i64_t));

    auto l_start = b.makeLabel();

    b.startBlock(l_start, cs2s("start"));

    auto int_part_ref = a_int_part_ptr.deref();

    b.gen(b.make_cast(v_int_part, b.makeConstRef(i64_t, void_ptr_t), a_x));
    b.gen(b.make_cast(int_part_ref, b.makeConstRef(f64_t, void_ptr_t), v_int_part));
    b.gen(b.make_sub(ret, a_x, int_part_ref));

    b.gen(b.make_ret());

    return funct;
}

ir::FunctId test_ir_intPart(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto f_modf = test_ir_modf(b);

    auto f64_t = type_get_numeric(Float64);

    auto args_t = type_get_tuple({&f64_t, 1});

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("intPart"), f64_t, args_t);

    auto modf_args_t = b.prog.functs[0].args_t;

    auto a_x = b.makeArgRef(0);

    auto ret = b.makeRetRef();

    auto v_0 = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_args = b.makeFrameRef(b.makeLocalVar(modf_args_t));

    auto v_arg0 =
        v_args.plus(type_tuple_offsetAt(modf_args_t, 0), type_tuple_typeAt(modf_args_t, 0));
    auto v_arg1 =
        v_args.plus(type_tuple_offsetAt(modf_args_t, 1), type_tuple_typeAt(modf_args_t, 1));

    auto l_start = b.makeLabel();

    b.startBlock(l_start, cs2s("start"));

    b.gen(b.make_mov(v_arg0, a_x));
    b.gen(b.make_lea(v_arg1, ret));
    b.gen(b.make_call(v_0, f_modf, v_args));

    b.gen(b.make_ret());

    return funct;
}

ir::FunctId test_ir_call3Times(ProgramBuilder &b, type_t fn) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto void_t = type_get_void();
    auto args_t = type_get_tuple({});

    auto i64_t = type_get_numeric(Int64);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("call3Times"), void_t, args_t);

    auto v_i = b.makeFrameRef(b.makeLocalVar(i64_t));
    auto v_cond = b.makeFrameRef(b.makeLocalVar(i64_t));

    auto l_start = b.makeLabel();
    auto l_loop = b.makeLabel();
    auto l_end = b.makeLabel();

    b.startBlock(l_start, cs2s("start"));

    b.gen(b.make_mov(v_i, b.makeConstRef(3ll, i64_t)));

    b.startBlock(l_loop, cs2s("loop"));

    b.gen(b.make_gt(v_cond, v_i, b.makeConstRef(0ll, i64_t)));
    b.gen(b.make_jmpz(v_cond, l_end));

    b.gen(b.make_call({}, b.makeConstRef({nullptr, fn}), {}));
    b.gen(b.make_sub(v_i, v_i, b.makeConstRef(1ll, i64_t)));
    b.gen(b.make_jmp(l_loop));

    b.startBlock(l_end, cs2s("end"));

    b.gen(b.make_ret());

    return funct;
}

ir::FunctId test_ir_hasZeroByte32(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto i32_t = type_get_numeric(Int32);

    ARRAY_SLICE_INIT(type_t, args, i32_t);
    auto args_t = type_get_tuple(args);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("hasZeroByte32"), i32_t, args_t);

    auto a_x = b.makeArgRef(0);

    auto ret = b.makeRetRef();

    auto r_a = b.makeRegRef(Reg_A, i32_t);

    auto l_start = b.makeLabel();

    b.startBlock(l_start, cs2s("start"));

    b.gen(b.make_sub(ret, a_x, b.makeConstRef(0x01010101u, i32_t)));
    b.gen(b.make_compl(r_a, a_x));
    b.gen(b.make_bitand(ret, ret, r_a));
    b.gen(b.make_bitand(ret, ret, b.makeConstRef(0x80808080u, i32_t)));

    b.gen(b.make_ret());

    return funct;
}

void test_ir_readToggleGlobal(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto void_t = type_get_void();
    auto args_t = type_get_tuple({});

    auto i64_t = type_get_numeric(Int64);

    auto g_var = b.makeGlobalRef(b.makeGlobalVar(i64_t));

    {
        b.startFunct(b.makeFunct(), cs2s("readGlobal"), i64_t, args_t);

        auto ret = b.makeRetRef();

        b.startBlock(b.makeLabel(), cs2s("start"));
        b.gen(b.make_mov(ret, g_var));
        b.gen(b.make_ret());
    }

    {
        b.startFunct(b.makeFunct(), cs2s("toggleGlobal"), void_t, args_t);

        b.startBlock(b.makeLabel(), cs2s("start"));
        b.gen(b.make_not(g_var, g_var));
        b.gen(b.make_ret());
    }
}

ir::FunctId test_ir_callNativeFunc(ProgramBuilder &b, void *fn_ptr) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto void_t = type_get_void();
    auto args_t = type_get_tuple({});

    auto fn_t = type_get_fn_native(void_t, args_t, 0, fn_ptr, false);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("callNativeFunc"), void_t, args_t);

    b.startBlock(b.makeLabel(), cs2s("start"));

    b.gen(b.make_call({}, b.makeConstRef({nullptr, fn_t}), {}));

    b.gen(b.make_ret());

    return funct;
}

ir::FunctId test_ir_callNativeAdd(ProgramBuilder &b, void *fn_ptr) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto i64_t = type_get_numeric(Int64);

    ARRAY_SLICE_INIT(type_t, args, i64_t, i64_t);
    auto args_t = type_get_tuple(args);

    auto fn_t = type_get_fn_native(i64_t, args_t, 0, fn_ptr, false);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("callNativeAdd"), i64_t, args_t);

    auto ret = b.makeRetRef();

    b.startBlock(b.makeLabel(), cs2s("start"));

    b.gen(b.make_call(ret, b.makeConstRef({nullptr, fn_t}), b.makeArgRef(0).as(args_t)));

    b.gen(b.make_ret());

    return funct;
}

ir::FunctId test_ir_callExternalPrintf(ProgramBuilder &b, string libname) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto void_t = type_get_void();
    auto i8_t = type_get_numeric(Int8);
    auto i32_t = type_get_numeric(Int32);
    auto i64_t = type_get_numeric(Int64);

    auto i8_ptr_t = type_get_ptr(i8_t);

    auto args_t = type_get_tuple({});

    ARRAY_SLICE_INIT(type_t, pf_args, i8_ptr_t, i64_t, i64_t, i64_t);
    auto pf_args_t = type_get_tuple(pf_args);

    auto f_printf = b.makeExtFunct(
        b.makeShObj(s2id(libname)),
        cs2id("test_printf"),
        i32_t,
        type_get_tuple({&i8_ptr_t, 1}),
        true);

    auto const funct = b.makeFunct();
    b.startFunct(funct, cs2s("callExternalPrintf"), void_t, args_t);

    auto args = b.makeFrameRef(b.makeLocalVar(pf_args_t));

    auto arg0 = args.plus(type_tuple_offsetAt(pf_args_t, 0), i8_ptr_t);
    auto arg1 = args.plus(type_tuple_offsetAt(pf_args_t, 1), i64_t);
    auto arg2 = args.plus(type_tuple_offsetAt(pf_args_t, 2), i64_t);
    auto arg3 = args.plus(type_tuple_offsetAt(pf_args_t, 3), i64_t);

    b.startBlock(b.makeLabel(), cs2s("start"));

    b.gen(b.make_mov(arg0, b.makeConstRef((char const *)"%lli + %lli = %lli\n", i8_ptr_t)));
    b.gen(b.make_mov(arg1, b.makeConstRef(4ll, i64_t)));
    b.gen(b.make_mov(arg2, b.makeConstRef(5ll, i64_t)));
    b.gen(b.make_add(arg3, arg1, arg2));
    b.gen(b.make_call(b.makeRegRef(Reg_A, i32_t), f_printf, args));

    b.gen(b.make_ret());

    return funct;
}

std::pair<ir::FunctId, ir::FunctId> test_ir_getSetExternalVar(ProgramBuilder &b, string libname) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto void_t = type_get_void();
    auto i64_t = type_get_numeric(Int64);

    auto args_t = type_get_tuple({});

    auto v_test_val =
        b.makeExtVarRef(b.makeExtVar(b.makeShObj(s2id(libname)), cs2id("test_val"), i64_t));

    auto const get_funct = b.makeFunct();
    b.startFunct(get_funct, cs2s("getExternalVar"), i64_t, args_t);

    auto ret = b.makeRetRef();

    b.startBlock(b.makeLabel(), cs2s("start"));
    b.gen(b.make_mov(ret, v_test_val));
    b.gen(b.make_ret());

    auto const set_funct = b.makeFunct();
    b.startFunct(set_funct, cs2s("setExternalVar"), void_t, args_t);

    b.startBlock(b.makeLabel(), cs2s("start"));
    b.gen(b.make_mov(v_test_val, b.makeConstRef(42ll, i64_t)));
    b.gen(b.make_ret());

    return std::make_pair(get_funct, set_funct);
}

void test_ir_main_argc(ProgramBuilder &b) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto i32_t = type_get_numeric(Int32);

    _startMain(b);

    auto ret = b.makeRetRef();

    auto a_argc = b.makeArgRef(0);

    auto l_error = b.makeLabel();

    b.gen(b.make_eq(b.makeRegRef(Reg_A, i32_t), a_argc, b.makeConstRef(3, i32_t)));
    b.gen(b.make_jmpz(b.makeRegRef(Reg_A, i32_t), l_error));
    b.gen(b.make_mov(ret, b.makeConstRef(0, i32_t)));
    b.gen(b.make_ret());

    b.startBlock(l_error, cs2s("error"));
    b.gen(b.make_mov(ret, b.makeConstRef(1, i32_t)));
    b.gen(b.make_ret());
}

void test_ir_main_pi(ProgramBuilder &b, string libc_name) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto f_pi = test_ir_pi(b);
    auto f_printf = _makePrintf(b, libc_name);

    auto i8_t = type_get_numeric(Int8);
    auto i32_t = type_get_numeric(Int32);
    auto f64_t = type_get_numeric(Float64);

    auto i8_ptr_t = type_get_ptr(i8_t);

    _startMain(b);

    ARRAY_SLICE_INIT(type_t, pf_args, i8_ptr_t, f64_t);
    auto pf_args_t = type_get_tuple(pf_args);

    auto v_pf_args = b.makeFrameRef(b.makeLocalVar(pf_args_t));

    auto arg0 = v_pf_args.plus(type_tuple_offsetAt(pf_args_t, 0), i8_ptr_t);
    auto arg1 = v_pf_args.plus(type_tuple_offsetAt(pf_args_t, 1), f64_t);

    auto fmt = cs2s("pi = %.16lf\n");
    auto fmt_t = type_get_ptr(type_get_array(i8_t, fmt.size));

    b.gen(b.make_mov(arg0, b.makeConstRef(fmt.data, fmt_t)));
    b.gen(b.make_call(arg1, f_pi, {}));
    b.gen(b.make_call(b.makeRegRef(Reg_A, i32_t), f_printf, v_pf_args));

    b.gen(b.make_ret());
}

void test_ir_main_vec2LenSquared(ProgramBuilder &b, string libc_name) {
    EASY_FUNCTION(profiler::colors::Grey200)

    auto f_vec2LenSquared = test_ir_vec2LenSquared(b);
    auto f_printf = _makePrintf(b, libc_name);

    auto i8_t = type_get_numeric(Int8);
    auto i32_t = type_get_numeric(Int32);
    auto f64_t = type_get_numeric(Float64);

    ARRAY_SLICE_INIT(type_t, vec_types, f64_t, f64_t);
    auto vec_t = type_get_tuple(vec_types);

    auto vec_ptr_t = type_get_ptr(vec_t);
    auto f64_ptr_t = type_get_ptr(f64_t);

    auto i8_ptr_t = type_get_ptr(i8_t);

    _startMain(b);

    auto v_vec = b.makeFrameRef(b.makeLocalVar(vec_t));
    auto v_lenSquared = b.makeFrameRef(b.makeLocalVar(f64_t));
    auto v_args = b.makeFrameRef(b.makeLocalVar(b.prog.functs[0].args_t));

    auto vec_arg0 = v_args.plus(type_tuple_offsetAt(vec_t, 0), vec_ptr_t);
    auto vec_arg1 = v_args.plus(type_tuple_offsetAt(vec_t, 1), f64_ptr_t);

    b.gen(b.make_mov(v_vec.plus(type_tuple_offsetAt(vec_t, 0), f64_t), b.makeConstRef(4.0, f64_t)));
    b.gen(b.make_mov(v_vec.plus(type_tuple_offsetAt(vec_t, 1), f64_t), b.makeConstRef(5.0, f64_t)));
    b.gen(b.make_lea(vec_arg0, v_vec));
    b.gen(b.make_lea(vec_arg1, v_lenSquared));
    b.gen(b.make_call({}, f_vec2LenSquared, v_args));

    ARRAY_SLICE_INIT(type_t, pf_args, i8_ptr_t, f64_t);
    auto pf_args_t = type_get_tuple(pf_args);

    auto v_pf_args = b.makeFrameRef(b.makeLocalVar(pf_args_t));

    auto pf_arg0 = v_pf_args.plus(type_tuple_offsetAt(pf_args_t, 0), i8_ptr_t);
    auto pf_arg1 = v_pf_args.plus(type_tuple_offsetAt(pf_args_t, 1), f64_t);

    auto fmt = cs2s("lenSquared = %lf\n");
    auto fmt_t = type_get_ptr(type_get_array(i8_t, fmt.size));

    b.gen(b.make_mov(pf_arg0, b.makeConstRef(fmt.data, fmt_t)));
    b.gen(b.make_mov(pf_arg1, v_lenSquared));
    b.gen(b.make_call(b.makeRegRef(Reg_A, i32_t), f_printf, v_pf_args));

    b.gen(b.make_ret());
}

} // namespace vm
} // namespace nk
