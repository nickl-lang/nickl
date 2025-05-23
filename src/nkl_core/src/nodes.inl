#ifndef XN
#define XN(N, T)
#endif

#ifndef X
#define X(N) XN(N, #N)
#endif

X(null)

X(nickl)

X(any_t)
X(bool)
X(nullptr)
X(type_t)
X(void)

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
X(usize)

X(array)
X(ptr)
X(slice)
X(struct)

X(escaped_string)
X(false)
X(float)
X(int)
X(string)
X(true)

X(add)
X(sub)
X(mul)
X(div)
X(mod)

X(eq)
X(ne)
X(lt)
X(le)
X(gt)
X(ge)

X(bitand)
X(bitor)
X(xor)
X(lsh)
X(rsh)

X(and)
X(not)
X(or)

X(addr)
X(assign)
X(call)
X(const)
X(context)
XN(defer_stmt, "defer")
X(deref)
X(export)
X(id)
X(if)
X(import)
X(index)
X(link)
X(list)
X(member)
X(param)
X(proc)
X(pub)
X(return)
X(run)
X(scope)
X(var)
X(while)

XN(ellipsis, "...")

#undef X
#undef XN
