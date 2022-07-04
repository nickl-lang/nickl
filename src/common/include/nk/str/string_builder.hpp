#ifndef HEADER_GUARD_NK_COMMON_I_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_I_STRING_BUILDER

#include <cstdint>
#include <type_traits>

#include "nk/mem/allocator.hpp"
#include "nk/str/string.hpp"

namespace nk {

struct StringBuilder {
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
    StringBuilder &operator<<(T val) {
        print(val);
        return *this;
    }

    virtual size_t size() const = 0;
    virtual int printf(char const *fmt, ...) = 0;

    string moveStr(Allocator &allocator);

    virtual string moveStr(Slice<char> dst) = 0;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_I_STRING_BUILDER
