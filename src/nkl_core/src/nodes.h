#ifndef NKL_CORE_NODES_H_
#define NKL_CORE_NODES_H_

#include "ntk/utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
#define XN(N, T) NK_CAT(n_, N),
#include "nodes.inl"

    node_count,
} NklAstNodeId;

#ifdef __cplusplus
}
#endif

#endif // NKL_CORE_NODES_H_
