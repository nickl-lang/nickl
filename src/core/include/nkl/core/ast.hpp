#ifndef HEADER_GUARD_NKL_CORE_AST
#define HEADER_GUARD_NKL_CORE_AST

#include "nk/ds/arena.hpp"
#include "nk/str/string_builder.hpp"
#include "nk/utils/id.hpp"
#include "nkl/core/token.hpp"

namespace nkl {

using namespace nk;

struct Node;
struct NodeArg;

using NodeRef = Node const *;
using TokenRef = Token const *;
using NodeArray = Slice<Node const>;
using NodeArgArray = Slice<NodeArg const>;

struct NodeArg {
    TokenRef token;
    NodeArray nodes;
};

struct Node {
    NodeArg arg[3];
    Id id;
};

struct PackedNodeArgArray : NodeArray {
    size_t size;

    PackedNodeArgArray(NodeArray ar);
    NodeArg const &operator[](size_t i) const;
};

struct Ast {
    Arena<Node> data;

    void init();
    void deinit();

    NodeArg push(TokenRef token);
    NodeArg push(NodeRef node);
    NodeArg push(TokenRef token, Node const &node);
    NodeArg push(Node const &node);
    NodeArg push(NodeArray nodes);
    NodeArg push(NodeArg arg);
    NodeArg push(NodeArgArray args);
};

StringBuilder &ast_inspect(NodeRef node, StringBuilder &sb);

static NodeRef const n_none = nullptr;

} // namespace nkl

#endif // HEADER_GUARD_NKL_CORE_AST
