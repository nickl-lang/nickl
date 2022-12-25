#ifndef HEADER_GUARD_NK_LANG_AST
#define HEADER_GUARD_NK_LANG_AST

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

struct Token {
    char const *text;
    size_t len;
    size_t id;
};

struct Node;

struct NodeArray {
    Node *data;
    size_t size;
};

struct Node {
    NodeArray args[3];
    Token const *token;
    size_t id;
};

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NK_LANG_AST
