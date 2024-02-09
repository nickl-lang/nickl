#ifndef OP
#define OP(...)
#endif

// id, text

OP(operator_marker, "")

// OP(elvis, "?:")
// OP(minus_2x, "--")
// OP(period_2x, "..")
// OP(plus_2x, "++")
// OP(uscore, "_")

OP(amper, "&")
OP(amper_2x, "&&")
OP(amper_eq, "&=")
OP(aster, "*")
OP(aster_eq, "*=")
OP(at, "@")
OP(bar, "|")
OP(bar_2x, "||")
OP(bar_eq, "|=")
OP(brace_l, "{")
OP(brace_2x, "{}")
OP(brace_r, "}")
OP(bracket_l, "[")
OP(bracket_2x, "[]")
OP(bracket_r, "]")
OP(caret, "^")
OP(caret_eq, "^=")
OP(colon, ":")
OP(colon_2x, "::")
OP(colon_eq, ":=")
OP(comma, ",")
OP(dollar, "$")
OP(eq, "=")
OP(eq_2x, "==")
OP(exclam, "!")
OP(exclam_eq, "!=")
OP(greater, ">")
OP(greater_2x, ">>")
OP(greater_2x_eq, ">>=")
OP(greater_eq, ">=")
OP(less, "<")
OP(less_2x, "<<")
OP(less_2x_eq, "<<=")
OP(less_eq, "<=")
OP(minus, "-")
OP(minus_eq, "-=")
OP(minus_greater, "->")
OP(number, "#")
OP(par_2x, "()")
OP(par_l, "(")
OP(par_r, ")")
OP(percent, "%")
OP(percent_eq, "%=")
OP(period, ".")
OP(period_aster, ".*")
OP(period_3x, "...")
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

// KW(new)
// KW(case)
// KW(default)
// KW(do)
// KW(switch)

KW(break)
KW(cast)
KW(const)
KW(continue)
KW(defer)
KW(else)
KW(enum)
KW(false)
KW(for)
KW(if)
KW(import)
KW(in)
KW(null)
KW(return )
KW(struct)
KW(true)
KW(union)
KW(while)

KW(typename_marker)

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

KW(any_t)
KW(bool)
KW(type_t)
KW(void)

#undef KW

#ifndef SP
#define SP(...)
#endif

// id, text

SP(id, "identifier")

SP(int, "int constant")
SP(int_uscore, "int constant")
SP(int_hex, "hex int constant")
SP(int_hex_uscore, "hex int constant")
SP(float, "float constant")
SP(string, "string constant")
SP(escaped_string, "string constant")
SP(tag, "tag")
SP(intrinsic, "intrinsic")

SP(empty, "")
SP(error, "")
SP(eof, "")

#undef SP
