#ifndef HEADER_GUARD_NK_COMMON_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_STRING_BUILDER

#include "nk/common/block_allocator.h"
#include "nk/common/mem.hpp"
#include "nk/common/string.hpp"

class StringBuilder {
public:
    void reserve(size_t n);

    int print(string str);
    int print(char const *str);
    int print(char c);

    StringBuilder &operator<<(string str) {
        print(str);
        return *this;
    }

    StringBuilder &operator<<(char const *str) {
        print(str);
        return *this;
    }

    StringBuilder &operator<<(char c) {
        print(c);
        return *this;
    }

    template <class T>
    StringBuilder &operator<<(T) {
        static_assert(!std::is_same_v<T, T>, "not implemented");
        return *this;
    }

    int printf(char const *fmt, ...);

    string moveStr(Allocator &allocator);

private:
    BlockAllocator m_allocator;
};

#endif // HEADER_GUARD_NK_COMMON_STRING_BUILDER
