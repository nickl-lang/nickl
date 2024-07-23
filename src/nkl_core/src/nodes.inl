#ifndef XN
#define XN(N, T)
#endif

#ifndef X
#define X(N) XN(N, #N)
#endif

X(null)

X(add)
X(and)
X(assign)
X(bitand)
X(bitor)
X(call)
X(const)
X(context)
X(escaped_string)
X(extern)
X(extern_c_proc)
X(f32)
X(f64)
X(float)
X(i16)
X(i32)
X(i64)
X(i8)
X(id)
X(if)
X(import)
X(int)
X(less)
X(list)
X(lsh)
X(member)
X(mul)
X(neq)
X(or)
X(param)
X(proc)
X(ptr)
X(pub)
X(return)
X(rsh)
X(run)
X(scope)
X(string)
X(struct)
X(var)
X(void)
X(while)
X(xor)

XN(ellipsis, "...")

#undef X
#undef XN
