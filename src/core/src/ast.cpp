#include "nkl/core/ast.hpp"

#include <algorithm>

#include "nk/utils/utils.hpp"

#define PACKED_ARG_ARRAY_SIZE(AR) roundUp((AR).size, c_arg_count) / c_arg_count
#define PACKED_ARG_ARRAY_AT(AR, IDX) (AR)[(IDX) / c_arg_count].arg[(IDX) % c_arg_count]

namespace nkl {

static constexpr size_t c_arg_count = sizeof(Node::arg) / sizeof(Node::arg[0]);

namespace {

NodeArg mkarg(Token const *token) {
    return {token, {}};
}

NodeArg mkarg(NodeRef node) {
    return {{}, {node, 1}};
}

NodeArg mkarg(Token const *token, NodeRef node) {
    return {token, {node, 1}};
}

NodeArg mkarg(NodeArray ar) {
    return {{}, ar};
}

NodeArg mkarg(NodeArg arg) {
    return arg;
}

NodeArg mkarg(PackedNodeArgArray ar) {
    return {{}, ar};
}

} // namespace

PackedNodeArgArray::PackedNodeArgArray(NodeArray ar)
    : Slice{ar}
    , size{
          ar.size > 0 ? (ar.size - 1) * 3 + std::count_if(
                                                std::begin(ar.back().arg),
                                                std::end(ar.back().arg),
                                                [](auto const &arg) {
                                                    return arg.token || arg.nodes.data;
                                                })
                      : 0} {
}

NodeArg const &PackedNodeArgArray::operator[](size_t i) const {
    return PACKED_ARG_ARRAY_AT(data, i);
}

void Ast::init() {
    data.reserve(1024);
}

void Ast::deinit() {
    data.deinit();
}

NodeRef Ast::gen(Node const &n) {
    return &(*data.push() = n);
}

NodeArg Ast::push(Token const *token) {
    return mkarg(token);
}

NodeArg Ast::push(NodeRef node) {
    return mkarg(node);
}

NodeArg Ast::push(Token const *token, Node const &node) {
    return mkarg(token, push(node).nodes);
}

NodeArg Ast::push(Node const &n) {
    return mkarg(gen(n));
}

NodeArg Ast::push(NodeArray ns) {
    return mkarg((NodeArray)ns.copy(data.push(ns.size)));
}

NodeArg Ast::push(NodeArg arg) {
    return push({&arg, 1});
}

NodeArg Ast::push(NodeArgArray args) {
    auto ar = data.push(PACKED_ARG_ARRAY_SIZE(args));
    std::fill_n(ar.begin(), ar.size, Node{});
    for (size_t i = 0; i < args.size; i++) {
        PACKED_ARG_ARRAY_AT(ar, i) = mkarg(args[i]);
    }
    return mkarg(PackedNodeArgArray{ar});
}

static void _ast_inspect(NodeRef node, StringBuilder &sb, size_t depth = 1) {
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

StringBuilder &ast_inspect(NodeRef node, StringBuilder &sb) {
    _ast_inspect(node, sb);
    return sb;
}

} // namespace nkl
