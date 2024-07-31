#ifndef IR
#define IR(NAME)
#endif

#ifndef UNA_IR
#define UNA_IR(NAME) IR(NAME)
#endif

#ifndef BIN_IR
#define BIN_IR(NAME) IR(NAME)
#endif

#ifndef DBL_IR
#define DBL_IR(NAME1, NAME2) IR(NAME1##_##NAME2)
#endif

#ifndef CMP_IR
#define CMP_IR(NAME) DBL_IR(cmp, NAME)
#endif

IR(nop) // nop

IR(ret) // ret arg

IR(jmp)   // jmp         %label
IR(jmpz)  // jmpz  cond, %label
IR(jmpnz) // jmpnz cond, %label

IR(call) // call proc, (args, ...)

UNA_IR(ext)   // ext   src -> dst
UNA_IR(trunc) // trunc src -> dst
UNA_IR(fp2i)  // fp2i  src -> dst
UNA_IR(i2fp)  // i2fp  src -> dst

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

CMP_IR(eq) // cmp eq lhs, rhs -> dst
CMP_IR(ne) // cmp ne lhs, rhs -> dst
CMP_IR(lt) // cmp lt lhs, rhs -> dst
CMP_IR(le) // cmp le lhs, rhs -> dst
CMP_IR(gt) // cmp gt lhs, rhs -> dst
CMP_IR(ge) // cmp ge lhs, rhs -> dst

IR(syscall) // syscall n, (args, ...)

// IR(cmpxchg)

IR(label)
IR(comment)

#undef CMP_IR
#undef DBL_IR
#undef BIN_IR
#undef UNA_IR

#undef IR
