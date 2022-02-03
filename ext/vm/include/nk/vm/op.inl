#ifndef X
#define X(...)
#endif

#ifndef I
#define I(...) X(__VA_ARGS__)
#endif

#ifndef U
#define U(...) X(__VA_ARGS__)
#endif

#ifndef B
#define B(...) X(__VA_ARGS__)
#endif

I(nop)
I(enter)
I(leave)
I(ret)

I(jmp)
I(jmpz)
I(jmpnz)

I(cast)

I(call)
I(call_jmp)

U(mov)
U(lea)
U(neg)
U(compl)
U(not)

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

#undef X
#undef I
#undef U
#undef B
