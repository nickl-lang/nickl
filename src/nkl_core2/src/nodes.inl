#ifndef XN
#define XN(N, T)
#endif

#ifndef X
#define X(N) XN(N, #N)
#endif

X(escaped_string)
X(float)
X(id)
X(int)
X(list)
X(string)

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

X(cast)

X(assign)
X(call)
X(const)
X(def)
X(extern)
X(param)
X(proc)
X(ptr)
X(pub)
X(return)
X(var)

X(false)
X(true)
X(nullptr)

X(void)
X(bool)

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
