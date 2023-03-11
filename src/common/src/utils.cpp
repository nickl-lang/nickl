#include "nk/common/utils.hpp"

#include <chrono>

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

std::string string_vformat(char const *fmt, va_list ap) {
    va_list ap_copy;

    va_copy(ap_copy, ap);
    int size_s = std::vsnprintf(nullptr, 0, fmt, ap_copy) + 1;
    va_end(ap_copy);

    auto size = static_cast<size_t>(size_s);
    std::string str;
    str.resize(size);

    va_copy(ap_copy, ap);
    std::vsnprintf(str.data(), size, fmt, ap_copy);
    va_end(ap_copy);

    return str;
}
