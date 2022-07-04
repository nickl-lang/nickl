#include "nk/common/string_builder.hpp"

#include <limits>
#include <type_traits>

#include "nk/common/allocator.hpp"
#include "nk/common/arena.hpp"
#include "nk/common/string.hpp"

namespace nk {

int StringBuilder::print(string str) {
    return printf("%.*s", str.size, str.data);
}

int StringBuilder::print(char const *str) {
    return print(cs2s(str));
}

int StringBuilder::print(char c) {
    return printf("%c", c);
}

int StringBuilder::print(int8_t val) {
    return printf("%hhi", val);
}

int StringBuilder::print(int16_t val) {
    return printf("%hi", val);
}

int StringBuilder::print(int32_t val) {
    return printf("%i", val);
}

int StringBuilder::print(int64_t val) {
    return printf("%lli", val);
}

int StringBuilder::print(uint8_t val) {
    return printf("%hhu", val);
}

int StringBuilder::print(uint16_t val) {
    return printf("%hu", val);
}

int StringBuilder::print(uint32_t val) {
    return printf("%u", val);
}

int StringBuilder::print(uint64_t val) {
    return printf("%llu", val);
}

int StringBuilder::print(float val) {
    return printf("%.*g", std::numeric_limits<float>::max_digits10, val);
}

int StringBuilder::print(double val) {
    return printf("%.*lg", std::numeric_limits<double>::max_digits10, val);
}

int StringBuilder::print(void *ptr) {
    return printf("%p", ptr);
}

string StringBuilder::moveStr(Allocator &allocator) {
    return moveStr(allocator.alloc<char>(size() + 1));
}

} // namespace nk
