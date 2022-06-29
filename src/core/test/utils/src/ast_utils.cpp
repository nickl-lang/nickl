#include "nkl/core/test/ast_utils.hpp"

namespace nkl {
namespace test {

bool ast_equal(NodeRef lhs, NodeRef rhs) {
    if (!lhs && !rhs) {
        return true;
    }

    if (!lhs || !rhs || lhs->id != rhs->id) {
        return false;
    }

    for (size_t i = 0; i < 3; i++) {
        auto const &larg = lhs->arg[i];
        auto const &rarg = rhs->arg[i];

        if (larg.token != rarg.token) {
            return false;
        }

        if (!larg.nodes.data && !rarg.nodes.data) {
            continue;
        }

        if (larg.nodes.size != rarg.nodes.size || !larg.nodes.data || !rarg.nodes.data) {
            return false;
        }

        for (size_t i = 0; i < larg.nodes.size; i++) {
            if (!ast_equal(&larg.nodes[i], &rarg.nodes[i])) {
                return false;
            }
        }
    }

    return true;
}

} // namespace test
} // namespace nkl
