#ifndef HEADER_GUARD_NKL_CORE_AST
#define HEADER_GUARD_NKL_CORE_AST

#include "nk/common/allocator.hpp"
#include "nk/common/arena.hpp"
#include "nk/common/id.hpp"
#include "nkl/core/token.hpp"

namespace nkl {

using namespace nk;

struct Node;
struct NamedNode;

using NodeRef = Node const *;
using NodeArray = Slice<Node const>;
using NamedNodeArray = Slice<NamedNode const>;

struct NamedNode {
    Token const *name;
    NodeRef node;
};

struct Node {
    struct Arg {
        Token const *token;
        NodeArray nodes;
    };
    Arg arg[3];
    Id id;
};

struct PackedNamedNodeArray : Slice<Node const> {
    PackedNamedNodeArray(Slice<Node const> ar)
        : Slice<Node const>{ar} {
    }

    Node::Arg const &operator[](size_t i);
};

struct Ast {
    Arena<Node> data;

    void init();
    void deinit();

    Node::Arg push(Token const *token);
    Node::Arg push(Node const &node);
    Node::Arg push(NodeArray nodes);
    Node::Arg push(NamedNode named_node);
    Node::Arg push(NamedNodeArray named_nodes);
};

string ast_inspect(NodeRef node, Allocator &allocator);

static NodeRef const n_none_ref = nullptr;

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_AST
