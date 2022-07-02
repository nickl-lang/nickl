#ifndef HEADER_GUARD_NK_VM_COMMON_TYPES
#define HEADER_GUARD_NK_VM_COMMON_TYPES

#include <cstddef>
#include <cstdint>

#include "nk/common/string.hpp"

namespace nk {
namespace vm {

struct Type;

using type_t = Type const *;

using typeid_t = size_t;
using typeclassid_t = uint8_t;

struct value_t {
    void *data;
    type_t type;
};

using FuncPtr = void (*)(type_t self, value_t ret, value_t args);

} // namespace vm
} // namespace nk

#endif // HEADER_GUARD_NK_VM_COMMON_TYPES
