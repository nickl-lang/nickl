#ifndef XN
#define XN(N, T)
#endif

#ifndef X
#define X(N) XN(N, #N)
#endif

X(null)

X(add)
X(addr)
X(and)
X(any_t)
X(assign)
X(bitand)
X(bitor)
X(call)
X(const)
X(context)
X(deref)
X(div)
X(eq)
X(escaped_string)
X(export)
X(extern)
X(f32)
X(f64)
X(false)
X(float)
X(ge)
X(gt)
X(i16)
X(i32)
X(i64)
X(i8)
X(id)
X(if)
X(import)
X(int)
X(le)
X(list)
X(lsh)
X(lt)
X(member)
X(mod)
X(mul)
X(ne)
X(nullptr)
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
X(sub)
X(true)
X(type_t)
X(u16)
X(u32)
X(u64)
X(u8)
X(var)
X(void)
X(while)
X(xor)

XN(ellipsis, "...")

#undef X
#undef XN
