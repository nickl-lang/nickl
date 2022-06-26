#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER_BASE
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER_BASE

#include <cstdint>
#include <type_traits>

#include "nk/common/allocator.hpp"
#include "nk/common/string.hpp"

namespace nk {

template <class TSelf>
struct StringBuilderBase {
    int print(string str);
    int print(char const *str);
    int print(char c);

    int print(int8_t val);
    int print(int16_t val);
    int print(int32_t val);
    int print(int64_t val);
    int print(uint8_t val);
    int print(uint16_t val);
    int print(uint32_t val);
    int print(uint64_t val);

    int print(float val);
    int print(double val);

    int print(void *ptr);

    template <class T>
    int print(T) {
        static_assert(!std::is_same_v<T, T>, "print not implemented");
        return 0;
    }

    template <class T>
    TSelf &operator<<(T val) {
        print(val);
        return self();
    }

private:
    TSelf &self();
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER_BASE
