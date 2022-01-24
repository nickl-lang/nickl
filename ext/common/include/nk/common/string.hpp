#ifndef HEADER_GUARD_NK_COMMON_STRGIN
#define HEADER_GUARD_NK_COMMON_STRGIN

#include <cstring>
#include <string_view>

struct string {
    char const *data;
    size_t size;

    std::string_view view() const {
        return std::string_view{data, size};
    }
};

inline string cstr_to_str(char const *str) {
    return string{str, std::strlen(str)};
}

#endif // HEADER_GUARD_NK_COMMON_STRGIN
