#ifndef HEADER_GUARD_NKL_COMMON_CONFIG
#define HEADER_GUARD_NKL_COMMON_CONFIG

#include "ntk/allocator.h"
#include "ntk/hash_tree.h"
#include "ntk/string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nks key;
    nks val;
} nks_KV;
nkht_typedef(nks_KV, nks_config);

bool readConfig(nks_config *conf, nks file);

#ifdef __cplusplus
}
#endif

#endif // HEADER_GUARD_NKL_COMMON_CONFIG
