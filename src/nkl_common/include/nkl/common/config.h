#ifndef NKL_COMMON_CONFIG_H_
#define NKL_COMMON_CONFIG_H_

#include "ntk/hash_tree_array.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

NK_HASH_TREE_ARRAY_FWD_KV(nks_config, NkString, NkString);

bool readConfig(nks_config *conf, NkString file);

#ifdef __cplusplus
}
#endif

#endif // NKL_COMMON_CONFIG_H_
