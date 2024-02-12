#ifndef NKL_COMMON_CONFIG_H_
#define NKL_COMMON_CONFIG_H_

#include "ntk/hash_tree.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    NkString key;
    NkString val;
} nks_kv;

NK_HASH_TREE_PROTO(nks_config, nks_kv, NkString);

bool readConfig(nks_config *conf, NkString file);

#ifdef __cplusplus
}
#endif

#endif // NKL_COMMON_CONFIG_H_
