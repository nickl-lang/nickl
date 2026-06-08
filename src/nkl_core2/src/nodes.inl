#ifndef XN
    #define XN(N, T)
#endif

#ifndef X
    #define X(N) XN(N, #N)
#endif

X(id)
X(int)
X(float)
X(string)
X(escaped_string)

XN(true_lit, "true")
XN(false_lit, "false")
X(nullptr)

X(list)

X(add)
X(sub)
X(mul)
X(div)
X(mod)

X(lsh)
X(rsh)
X(xor)
X(bitor)
X(bitand)

X(lt)
X(gt)
X(le)
X(ge)
X(eq)
X(ne)

X(and)
X(or)
X(not)

X(cast)

X(addr)
X(deref)
X(member)

X(assign)
X(call)
X(const)
X(def)
X(extern)
X(if)
X(param)
X(proc)
X(ptr)
X(pub)
X(return)
X(struct)
X(var)
X(while)

X(void)
XN(boolean, "bool")

X(i8)
X(i16)
X(i32)
X(i64)
X(u8)
X(u16)
X(u32)
X(u64)
X(f32)
X(f64)

XN(ellipsis, "...")

#undef X
#undef XN
