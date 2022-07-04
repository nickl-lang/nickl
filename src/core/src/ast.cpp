#include "nkl/core/ast.hpp"

#include <algorithm>

#include "nk/common/dynamic_string_builder.hpp"
#include "nk/common/utils.hpp"

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

Node::Arg mkarg(Token const *token, NodeRef n) {
    return {token, {n, 1}};
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

PackedNamedNodeArray::PackedNamedNodeArray(Slice<Node const> ar)
    : Slice{ar}
    , size{
          ar.size > 0 ? (ar.size - 1) * 3 + std::count_if(
                                                std::begin(ar.back().arg),
                                                std::end(ar.back().arg),
                                                [](auto const &arg) {
                                                    return arg.token && arg.nodes.data;
                                                })
                      : 0} {
}

NamedNode PackedNamedNodeArray::operator[](size_t i) const {
    auto const &arg = PACKED_NN_ARRAY_AT(data, i);
    return {arg.token, arg.nodes.begin()};
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

Node::Arg Ast::push(Token const *token, Node const &node) {
    return mkarg(token, push(node).nodes);
}

Node::Arg Ast::push(Node const &n) {
    return mkarg(&(*data.push() = n));
}

Node::Arg Ast::push(NodeArray ns) {
    auto ar = (NodeArray)ns.copy(data.push(ns.size));
    auto arg = mkarg(ar);
    return arg;
}

Node::Arg Ast::push(NamedNode n) {
    return push({&n, 1});
}

Node::Arg Ast::push(NamedNodeArray ns) {
    auto ar = data.push(PACKED_NN_ARRAY_SIZE(ns));
    std::fill_n(ar.begin(), ar.size, Node{});
    for (size_t i = 0; i < ns.size; i++) {
        PACKED_NN_ARRAY_AT(ar, i) = mkarg(ns[i]);
    }
    return mkarg(PackedNamedNodeArray{ar});
}

static void _ast_inspect(NodeRef node, DynamicStringBuilder &sb, size_t depth = 1) {
    if (!node) {
        sb << "(null)";
        return;
    }

    auto const newline = [&](size_t depth) {
        sb << '\n';
        sb.printf("%*s", 2 * depth, "");
    };

    sb << "{";
    defer {
        sb << "}";
    };

    if (node->id) {
        newline(depth);
        string const str = id2s(node->id);
        sb << "id: ";
        if (str.data) {
            sb << "\"" << str << "\"";
        } else {
            sb << cs2s("(null)");
        }
    }

    for (size_t i = 0; i < 3; i++) {
        auto const &arg = node->arg[i];
        if (!arg.token && !arg.nodes.size) {
            continue;
        }

        if (node->id) {
            sb << ',';
        }
        newline(depth);
        sb << "arg" << i << ": ";

        if (arg.token && arg.nodes.size) {
            sb << "[";
        }
        defer {
            if (arg.token && arg.nodes.size) {
                sb << "]";
            }
        };

        if (arg.token) {
            if (arg.nodes.size) {
                newline(depth + 1);
            }
            sb << "\"" << arg.token->text << "\"";
        }

        if (arg.token && arg.nodes.size) {
            sb << ", ";
        }

        if (arg.nodes.size > 1) {
            sb << "[";
        }
        defer {
            if (arg.nodes.size > 1) {
                sb << "]";
            }
        };

        bool first = true;
        for (auto const &child : arg.nodes) {
            if (!first) {
                sb << ", ";
            }
            first = false;
            _ast_inspect(&child, sb, depth + 1);
        }
    }
}

string ast_inspect(NodeRef node, Allocator &allocator) {
    DynamicStringBuilder sb{};
    sb.reserve(1000);
    _ast_inspect(node, sb);
    return sb.moveStr(allocator);
}

} // namespace nkl
