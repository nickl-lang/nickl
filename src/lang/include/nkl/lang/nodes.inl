#ifndef X
#define X(...)
#endif

// id

#ifndef N
#define N(...) X(__VA_ARGS__)
#endif

N(none) // ()

N(nop)

N(break)
N(continue)

N(false)
N(true)

#undef N

#ifndef U
#define U(...) X(__VA_ARGS__)
#endif

U(compl ) // (arg: node)
U(not )
U(uminus)
U(uplus)

U(addr)  // (expr: node)
U(deref) // (ptr: node)

U(defer_stmt) // (expr: node)

U(return ) // (expr: node)

U(ptr_type) // (target_type: node)
U(const_ptr_type)
U(slice_type)

U(scope) // (expr: node)

#undef U

#ifndef B
#define B(...) X(__VA_ARGS__)
#endif

B(add) // (lhs: node, rhs: node)
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

B(add_assign)
B(sub_assign)
B(mul_assign)
B(div_assign)
B(mod_assign)

B(bitand_assign)
B(bitor_assign)
B(xor_assign)
B(lsh_assign)
B(rsh_assign)

B(array_type) // (lhs: node, rhs: node)
B(cast)       // (target_type: node, expr: node)
B(index)      // (lhs: node, index: node)
B(while)      // (cond: node, body: node)

#undef B

X(if) // (cond: node, then_clause: node, else_clause: node)
X(ternary)

X(array) // (nodes: [node])
X(block)
X(tuple)
X(tuple_type)
X(run)

X(import) // (names: [{name: token}])

X(id)                     // (name: token)
X(numeric_float)          // (value: token)
X(numeric_int)            // (value: token)
X(string_literal)         // (value: token)
X(escaped_string_literal) // (value: token)
X(import_path)            // (path: token)
X(typename)               // (name: token)

X(for) // (it: token, range: node, body: node)
X(for_by_ptr)

X(member) // (lhs: node, name: token)

X(struct) // (fields: [(id=mut|const name: token, type: node, value: node)])
X(union)
X(enum)
X(packed_struct)

X(fn) // (params: [(id=mut|const name: token, type: node, value: node)], ret_t: node, body: ?node)

X(tag) // (tag: token, args: [{name: ?token, value: node}], node: node)

X(call) // (lhs: ?node, args: [{name: ?token, value: node}])
X(object_literal)

X(assign) // (lhs: [node], value: node)

X(define) // (names: [{name: token}], value: node)

X(comptime_const_def) // (name: token, value: node)
X(tag_def)

X(var_decl) // (name: token, type: node, value: ?node)
X(const_decl)

#undef X
