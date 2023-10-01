#include "ntk/utils.hpp"

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
