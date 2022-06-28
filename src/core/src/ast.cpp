#include "nkl/core/ast.hpp"

#define PACKED_NN_ARRAY_SIZE(AR) roundUp((AR).size, c_arg_count) / c_arg_count
#define PACKED_NN_ARRAY_AT(AR, IDX) (AR)[(IDX) / c_arg_count].arg[(IDX) % c_arg_count]

namespace nkl {

static constexpr size_t c_arg_count = sizeof(Node::arg) / sizeof(Node::arg[0]);

namespace {

Node::Arg mkarg(Token const *token) {
    return {token, {}};
}

Node::Arg mkarg(NodeRef n) {
    return {{}, {n, 1}};
}

Node::Arg mkarg(NodeArray ar) {
    return {{}, ar};
}

Node::Arg mkarg(NamedNode nn) {
    return {nn.name, {nn.node, 1}};
}

Node::Arg mkarg(PackedNamedNodeArray ar) {
    return {{}, ar};
}

} // namespace

Node::Arg const &PackedNamedNodeArray::operator[](size_t i) {
    return PACKED_NN_ARRAY_AT(data, i);
}

void Ast::init() {
    data.reserve(1024);
}

void Ast::deinit() {
    data.deinit();
}

Node::Arg Ast::push(Token const *token) {
    return mkarg(token);
}

Node::Arg Ast::push(Node const &n) {
    return mkarg(&(*data.push() = n));
}

Node::Arg Ast::push(NodeArray ns) {
    return mkarg((NodeArray)ns.copy(data.push(ns.size)));
}

Node::Arg Ast::push(NamedNode n) {
    return push(n);
}

Node::Arg Ast::push(NamedNodeArray ns) {
    auto ar = data.push(PACKED_NN_ARRAY_SIZE(ns));
    for (size_t i = 0; i < ns.size; i++) {
        PACKED_NN_ARRAY_AT(ar, i) = mkarg(ns[i]);
    }
    return mkarg(PackedNamedNodeArray{ar});
}

} // namespace nkl
