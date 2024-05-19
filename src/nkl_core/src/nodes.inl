#ifndef XN
#define XN(N, T)
#endif

#ifndef X
#define X(N) XN(N, #N)
#endif

X(null)

X(add)
X(arg)
X(assign)
X(call)
X(context)
X(define)
X(escaped_string)
X(extern)
X(i32)
X(id)
X(import)
X(int)
X(less)
X(list)
X(proc)
X(ptr)
X(pub)
X(return)
X(run)
X(scope)
X(string)
X(void)
X(while)

XN(ellipsis, "...")

#undef X
#undef XN
