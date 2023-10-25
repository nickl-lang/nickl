#ifndef HEADER_GUARD_NKL_COMMON_CONFIG
#define HEADER_GUARD_NKL_COMMON_CONFIG

#include "ntk/allocator.h"
#include "ntk/hash_map.hpp"
#include "ntk/string.h"

bool readConfig(NkHashMap<nks, nks> &conf, NkAllocator alloc, nks file);

#endif // HEADER_GUARD_NKL_COMMON_CONFIG
