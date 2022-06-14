#include "nk/common/utils.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

hash_t hash_seed() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

hash_t hash_array(uint8_t const *begin, uint8_t const *end) {
    static const hash_t seed = hash_seed();
    hash_t hash = seed;
    static constexpr size_t c_step = sizeof(size_t);
    size_t i = 0;
    while (begin + (i + 1) * c_step <= end) {
        hash_combine(&hash, *(size_t const *)(begin + i++ * c_step));
    }
    i *= c_step;
    while (begin + i < end) {
        hash_combine(&hash, begin[i++]);
    }
    return hash;
}
