#ifndef OP
#define OP(...)
#endif

// id, text

OP(operator_marker, "")

OP(amper, "&")
OP(amper_dbl, "&&")
OP(amper_dbl_eq, "&&=")
OP(amper_eq, "&=")
OP(aster, "*")
OP(aster_eq, "*=")
// OP(at, "@")
OP(bar, "|")
OP(bar_dbl, "||")
OP(bar_dbl_eq, "||=")
OP(bar_eq, "|=")
// OP(brace_dbl, "{}")
OP(brace_l, "{")
OP(brace_r, "}")
// OP(bracket_dbl, "[]")
OP(bracket_l, "[")
OP(bracket_r, "]")
OP(caret, "^")
OP(caret_eq, "^=")
OP(colon, ":")
OP(colon_eq, ":=")
OP(comma, ",")
// OP(dollar, "$")
OP(eq, "=")
OP(eq_dbl, "==")
OP(exclam, "!")
OP(exclam_eq, "!=")
OP(greater, ">")
OP(greater_dbl, ">>")
OP(greater_dbl_eq, ">>=")
OP(greater_eq, ">=")
OP(less, "<")
OP(less_dbl, "<<")
OP(less_dbl_eq, "<<=")
OP(less_eq, "<=")
OP(minus, "-")
// OP(minus_dbl, "--")
OP(minus_eq, "-=")
OP(minus_greater, "->")
// OP(number, "#")
// OP(par_dbl, "()")
OP(par_l, "(")
OP(par_r, ")")
OP(percent, "%")
OP(percent_eq, "%=")
OP(period, ".")
// OP(period_dbl, "..")
OP(plus, "+")
// OP(plus_dbl, "++")
OP(plus_eq, "+=")
OP(question, "?")
// OP(elvis, "?:")
OP(semi, ";")
OP(slash, "/")
OP(slash_eq, "/=")
OP(tilde, "~")
// OP(underscore, "_")

#undef OP

#ifndef KW
#define KW(...)
#endif

// id

KW(keyword_marker)

// KW(alignof)
KW(break)
// KW(case)
KW(cast)
// KW(const)
KW(continue)
// KW(default)
// KW(do)
KW(else)
KW(false)
KW(fn)
KW(for)
KW(if)
// KW(in)
// KW(new)
// KW(offsetof)
KW(return)
// KW(sizeof)
KW(struct)
// KW(switch)
KW(true)
// KW(typeof)
// KW(union)
KW(while)

KW(typename_marker)

KW(void)

KW(i8)
KW(i16)
KW(i32)
KW(i64)
KW(u8)
KW(u16)
KW(u32)
KW(u64)
KW(f32)
KW(f64)

// KW(int)
// KW(uint)

// KW(string)

KW(tuple_t)
KW(typeref_t)
KW(array_t)
KW(ptr_t)

#undef KW

#ifndef SP
#define SP(...)
#endif

// id

SP(id)

SP(int_const)
SP(float_const)
SP(str_const)

SP(empty)
SP(error)
SP(eof)

#undef SP
