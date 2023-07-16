#include "nk/common/string.hpp"

#include <cstring>

nkstr nk_mkstr(char const *str) {
    return {str, std::strlen(str)};
}
