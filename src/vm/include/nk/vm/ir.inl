#ifndef X
#define X(...)
#endif

#ifndef I
#define I(...) X(__VA_ARGS__)
#endif

I(nop)   // nop
I(enter) // enter
I(leave) // leave
I(ret)   // ret

I(jmp)   // jmp %label
I(jmpz)  // jmpz $cond %label
I(jmpnz) // jmpnz $cond %label

I(cast) // $dst := cast $type $arg

I(call) // $dst := call $fn $args

#undef I

#ifndef U
#define U(...) X(__VA_ARGS__)
#endif

U(mov)    // $dst := mov $arg
U(lea)    // $dst := lea $arg
U(neg)    // $dst := neg $arg
U(compl ) // $dst := compl $arg
U(not )   // $dst := not $arg

#undef U

#ifndef B
#define B(...) X(__VA_ARGS__)
#endif

B(add) // $dst := add $lhs $rhs
B(sub) // $dst := sub $lhs $rhs
B(mul) // $dst := mul $lhs $rhs
B(div) // $dst := div $lhs $rhs
B(mod) // $dst := mod $lhs $rhs

B(bitand) // $dst := bitand $lhs $rhs
B(bitor)  // $dst := bitor $lhs $rhs
B(xor)    // $dst := xor $lhs $rhs
B(lsh)    // $dst := lsh $lhs $rhs
B(rsh)    // $dst := rsh $lhs $rhs

B(and) // $dst := and $lhs $rhs
B(or)  // $dst := or $lhs $rhs

B(eq) // $dst := eq $lhs $rhs
B(ge) // $dst := ge $lhs $rhs
B(gt) // $dst := gt $lhs $rhs
B(le) // $dst := le $lhs $rhs
B(lt) // $dst := lt $lhs $rhs
B(ne) // $dst := ne $lhs $rhs

#undef B

#undef X
