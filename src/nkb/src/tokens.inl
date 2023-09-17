#ifndef OP
#define OP(...)
#endif

// id, text

OP(operator_marker, "")

OP(amper, "&")
OP(aster, "*")
OP(brace_l, "{")
OP(brace_r, "}")
OP(bracket_l, "[")
OP(bracket_r, "]")
OP(colon, ":")
OP(comma, ",")
OP(minus_greater, "->")
OP(par_l, "(")
OP(par_r, ")")
OP(period_3x, "...")
OP(plus, "+")

#undef OP

#ifndef KW
#define KW(...)
#endif

// id

KW(keyword_marker)

KW(cdecl)
KW(const)
KW(data)
KW(extern)
KW(proc)
KW(type)

KW(nop)

KW(ret)

KW(jmp)
KW(jmpz)
KW(jmpnz)

KW(ext)
KW(trunc)
KW(fp2i)
KW(i2fp)

KW(call)

KW(mov)
KW(lea)

KW(neg)
KW(add)
KW(sub)
KW(mul)
KW(div)
KW(mod)

KW(and)
KW(or)
KW(xor)
KW(lsh)
KW(rsh)

KW(cmp)

KW(eq)
KW(ne)
KW(lt)
KW(le)
KW(gt)
KW(ge)

KW(typename_marker)

KW(void)

KW(f32)
KW(f64)
KW(i16)
KW(i32)
KW(i64)
KW(i8)
KW(u16)
KW(u32)
KW(u64)
KW(u8)

#undef KW

#ifndef SP
#define SP(...)
#endif

// id, text

SP(id, "identifier")

SP(newline, "newline")
SP(int, "int constant")
SP(float, "float constant")
SP(string, "string constant")
SP(escaped_string, "string constant")
SP(label, "label")

SP(empty, "")
SP(error, "error")
SP(eof, "end of file")

#undef SP
