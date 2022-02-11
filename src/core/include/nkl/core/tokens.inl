#ifndef OP
#define OP(...)
#endif

// id, text

OP(operator_marker, "")

// OP(at, "@")
// OP(brace_dbl, "{}")
// OP(bracket_dbl, "[]")
// OP(dollar, "$")
// OP(elvis, "?:")
// OP(minus_dbl, "--")
// OP(number, "#")
// OP(par_dbl, "()")
// OP(period_dbl, "..")
// OP(plus_dbl, "++")
// OP(underscore, "_")

OP(amper, "&")
OP(amper_dbl, "&&")
OP(amper_dbl_eq, "&&=")
OP(amper_eq, "&=")
OP(aster, "*")
OP(aster_eq, "*=")
OP(bar, "|")
OP(bar_dbl, "||")
OP(bar_dbl_eq, "||=")
OP(bar_eq, "|=")
OP(brace_l, "{")
OP(brace_r, "}")
OP(bracket_l, "[")
OP(bracket_r, "]")
OP(caret, "^")
OP(caret_eq, "^=")
OP(colon, ":")
OP(colon_dbl, "::")
OP(colon_eq, ":=")
OP(comma, ",")
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
OP(minus_eq, "-=")
OP(minus_greater, "->")
OP(par_l, "(")
OP(par_r, ")")
OP(percent, "%")
OP(percent_eq, "%=")
OP(period, ".")
OP(plus, "+")
OP(plus_eq, "+=")
OP(question, "?")
OP(semi, ";")
OP(slash, "/")
OP(slash_eq, "/=")
OP(tilde, "~")

#undef OP

#ifndef KW
#define KW(...)
#endif

// id

KW(keyword_marker)

// KW(()w)
// KW(alignof)
// KW(case)
// KW(const)
// KW(default)
// KW(do)
// KW(in)
// KW(offsetof)
// KW(sizeof)
// KW(switch)
// KW(typeof)
// KW(union)

KW(array_t)
KW(break)
KW(cast)
KW(continue)
KW(else)
KW(false)
KW(fn)
KW(for)
KW(foreign)
KW(if)
KW(ptr_t)
KW(return)
KW(struct)
KW(true)
KW(tuple_t)
KW(type_t)
KW(while)

KW(typename_marker)

// KW(int)
// KW(uint)
// KW(string)

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
