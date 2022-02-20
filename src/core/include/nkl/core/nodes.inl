#ifndef X
#define X(...)
#endif

// type, id

#ifndef N
#define N(...) X(__VA_ARGS__)
#endif

N(null, none)

N(null, nop)

N(null, break)
N(null, continue)

N(null, i8)
N(null, i16)
N(null, i32)
N(null, i64)
N(null, u8)
N(null, u16)
N(null, u32)
N(null, u64)
N(null, f32)
N(null, f64)

N(null, typeref)
N(null, void)

N(null, true)
N(null, false)

#undef N

#ifndef U
#define U(...) X(__VA_ARGS__)
#endif

U(unary, addr)
U(unary, deref)

U(unary, compl)
U(unary, not)
U(unary, uminus)
U(unary, uplus)

U(unary, return)

U(unary, ptr_type)

#undef U

#ifndef B
#define B(...) X(__VA_ARGS__)
#endif

B(binary, add)
B(binary, sub)
B(binary, mul)
B(binary, div)
B(binary, mod)

B(binary, bitand)
B(binary, bitor)
B(binary, xor)
B(binary, lsh)
B(binary, rsh)

B(binary, and)
B(binary, or)

B(binary, eq)
B(binary, ge)
B(binary, gt)
B(binary, le)
B(binary, lt)
B(binary, ne)

B(binary, add_assign)
B(binary, sub_assign)
B(binary, mul_assign)
B(binary, div_assign)
B(binary, mod_assign)

B(binary, bitand_assign)
B(binary, bitor_assign)
B(binary, xor_assign)
B(binary, lsh_assign)
B(binary, rsh_assign)

B(binary, and_assign)
B(binary, or_assign)

B(binary, array_type)
B(binary, assign)
B(binary, cast)
B(binary, colon_assign)
B(binary, index)
B(binary, tuple_index)
B(binary, while)

#undef B

X(ternary, if)
X(ternary, ternary)

X(array, array)
X(array, block)
X(array, tuple)
X(array, id_tuple)
X(array, tuple_type)

X(token, id)
X(token, numeric_float)
X(token, numeric_int)
X(token, string_literal)

X(member, member)

X(type_decl, struct)

X(fn, fn)
X(fn, foreign_fn)

X(call, call)
X(struct_literal, struct_literal)
X(var_decl, var_decl)

#undef X
