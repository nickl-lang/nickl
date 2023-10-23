#ifndef HEADER_GUARD_NKIRC_AST
#define HEADER_GUARD_NKIRC_AST

#include <stdint.h>

#include "ntk/id.h"

#ifndef __cplusplus
extern "C" {
#endif

typedef struct {
    nkid id;
    uint32_t token_idx;
    uint32_t total_children;
    uint32_t arity;
} NkAstNode;

#ifndef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKIRC_AST
