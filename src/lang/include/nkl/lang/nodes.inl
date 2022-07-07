#ifndef X
#define X(...)
#endif

// id

#ifndef N
#define N(...) X(__VA_ARGS__)
#endif

N(none)

N(nop)

N(break)
N(continue)

N(i8)
N(i16)
N(i32)
N(i64)
N(u8)
N(u16)
N(u32)
N(u64)
N(f32)
N(f64)

N(typeref)
N(void)

N(true)
N(false)

#undef N

#ifndef U
#define U(...) X(__VA_ARGS__)
#endif

U(addr)
U(deref)

U(compl)
U(not)
U(uminus)
U(uplus)

U(return)

U(ptr_type)

U(scope)

#undef U

#ifndef B
#define B(...) X(__VA_ARGS__)
#endif

B(add)
B(sub)
B(mul)
B(div)
B(mod)

B(bitand)
B(bitor)
B(xor)
B(lsh)
B(rsh)

B(and)
B(or)

B(eq)
B(ge)
B(gt)
B(le)
B(lt)
B(ne)

B(add_assign)
B(sub_assign)
B(mul_assign)
B(div_assign)
B(mod_assign)

B(bitand_assign)
B(bitor_assign)
B(xor_assign)
B(lsh_assign)
B(rsh_assign)

B(and_assign)
B(or_assign)

B(array_type)
B(assign)
B(cast)
B(colon_assign)
B(index)
B(tuple_index)
B(while)

#undef B

X(if)
X(ternary)

X(array)
X(block)
X(tuple)
X(id_tuple)
X(tuple_type)

X(id)
X(numeric_float)
X(numeric_int)
X(string_literal)
X(escaped_string_literal)

X(member)

X(struct)

X(fn)
X(foreign_fn)

X(call)
X(struct_literal)
X(var_decl)

#undef X
