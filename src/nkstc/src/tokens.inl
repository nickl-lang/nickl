#ifndef OP
#define OP(...)
#endif

// id, text

OP(operator_marker, "")

OP(bracket_l, "[")
OP(bracket_r, "]")
OP(par_l, "(")
OP(par_r, ")")

#undef OP

#ifndef SP
#define SP(...)
#endif

// id, text

SP(id, "identifier")

SP(int, "int constant")
SP(float, "float constant")
SP(string, "string constant")
SP(escaped_string, "string constant")

SP(empty, "")
SP(error, "error")
SP(eof, "end of file")

#undef SP
