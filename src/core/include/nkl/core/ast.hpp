#ifndef HEADER_GUARD_NKL_CORE_AST
#define HEADER_GUARD_NKL_CORE_AST

#include "nk/common/allocator.hpp"
#include "nk/common/arena.hpp"
#include "nkl/core/token.hpp"

namespace nkl {

using namespace nk;

struct Node;
using node_ref_t = Node const *;

struct Node {
    enum EArgType {
        Arg_None = 0,
        Arg_Token,
        Arg_Nodes,
    };
    struct Arg {
        union {
            Token const *token;
            Slice<Node const> nodes;
        } as;
        EArgType arg_type;
    };
    Arg arg[3];
    size_t id;
};

struct Ast {
    Arena<Node> data;

    void init();
    void deinit();

    // node_ref_t push(Node node);
    // NodeArray push_ar(NodeArray nodes);
    // NodeArray push_named_ar(NamedNodeArray nodes);
};

string ast_inspect(node_ref_t node, Allocator &allocator);

static node_ref_t const n_none_ref = nullptr;

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_AST
