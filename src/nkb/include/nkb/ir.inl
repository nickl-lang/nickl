#ifndef IR
#define IR(NAME)
#endif

#ifndef DBL_IR
#define DBL_IR(NAME1, NAME2) IR(NAME1##_##NAME2)
#endif

#ifndef UNA_IR
#define UNA_IR(NAME) IR(NAME)
#endif

#ifndef BIN_IR
#define BIN_IR(NAME) IR(NAME)
#endif

IR(nop)

IR(ret)

IR(jmp)   // jmp         %label
IR(jmpz)  // jmpz  cond, %label
IR(jmpnz) // jmpnz cond, %label

IR(call) // call proc, (args, ...)

UNA_IR(ext)   // ext   src -> dst
UNA_IR(trunc) // trunc src -> dst
UNA_IR(fp2i)  // fp2i  src -> dst
UNA_IR(i2fp)  // i2fp  src -> dst

UNA_IR(neg) // neg arg -> dst

UNA_IR(mov) // mov src -> dst
UNA_IR(lea) // lea src -> dst

BIN_IR(add) // add lhs, rhs -> dst
BIN_IR(sub) // sub lhs, rhs -> dst
BIN_IR(mul) // mul lhs, rhs -> dst
BIN_IR(div) // div lhs, rhs -> dst
BIN_IR(mod) // mod lhs, rhs -> dst

BIN_IR(and) // and lhs, rhs -> dst
BIN_IR(or)  // or  lhs, rhs -> dst
BIN_IR(xor) // xor lhs, rhs -> dst
BIN_IR(lsh) // lsh lhs, rhs -> dst
BIN_IR(rsh) // rsh lhs, rhs -> dst

DBL_IR(cmp, eq) // cmp eq lhs, rhs -> dst
DBL_IR(cmp, ne) // cmp ne lhs, rhs -> dst
DBL_IR(cmp, lt) // cmp lt lhs, rhs -> dst
DBL_IR(cmp, le) // cmp le lhs, rhs -> dst
DBL_IR(cmp, gt) // cmp gt lhs, rhs -> dst
DBL_IR(cmp, ge) // cmp ge lhs, rhs -> dst

// IR(cmpxchg)

IR(label)

#undef BIN_IR
#undef UNA_IR
#undef DBL_IR

#undef IR
