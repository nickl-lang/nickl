#ifndef XN
#define XN(N, T)
#endif

#ifndef X
#define X(N) XN(N, #N)
#endif

X(null)

X(arg)
X(assign)
X(call)
X(define)
X(extern)
X(less)
X(list)
X(proc)
X(ptr)
X(pub)
X(return )
X(while)

XN(ellipsis, "...")

#undef X
#undef XN