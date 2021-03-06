#include "nk/common/string_builder.hpp"

#include <cstdarg>
#include <limits>

#include "nk/common/utils.hpp"

static constexpr size_t PRINTF_BUFFER_SIZE = 4096;

void StringBuilder::reserve(size_t n) {
    BlockAllocator_reserve(&m_allocator, n);
}

int StringBuilder::print(string str) {
    str.copy({(char *)BlockAllocator_push(&m_allocator, str.size), str.size});
    return str.size;
}

int StringBuilder::print(char const *str) {
    return print(cs2s(str));
}

int StringBuilder::print(char c) {
    return print({&c, 1});
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

int StringBuilder::printf(char const *fmt, ...) {
    char buf[PRINTF_BUFFER_SIZE];

    va_list ap;
    va_start(ap, fmt);
    int const printf_res = vsnprintf(buf, PRINTF_BUFFER_SIZE, fmt, ap);
    int const byte_count = printf_res + 1;
    va_end(ap);

    if (byte_count > (int)PRINTF_BUFFER_SIZE) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf((char *)BlockAllocator_push(&m_allocator, byte_count), byte_count, fmt, ap);
        va_end(ap);

        BlockAllocator_pop(&m_allocator, 1);
    } else if (byte_count > 0) {
        ::memcpy(BlockAllocator_push(&m_allocator, byte_count - 1), buf, byte_count - 1);
    }

    return printf_res;
}

string StringBuilder::moveStr(Allocator &allocator) {
    *this << '\0';
    size_t const byte_count = m_allocator.size;
    char *mem = (char *)allocator.alloc(byte_count);
    BlockAllocator_copy(&m_allocator, (uint8_t *)mem);
    BlockAllocator_deinit(&m_allocator);
    return {mem, byte_count - 1};
}
