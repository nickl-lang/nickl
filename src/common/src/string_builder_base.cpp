#include "nk/common/private/string_builder_base.hpp"

#include <limits>
#include <type_traits>

#include "nk/common/allocator.hpp"
#include "nk/common/arena.hpp"
#include "nk/common/static_string_builder.hpp"
#include "nk/common/string.hpp"
#include "nk/common/string_builder.hpp"

namespace nk {

template struct StringBuilderBase<StringBuilder>;
template struct StringBuilderBase<StaticStringBuilder>;

template <class TSelf>
int StringBuilderBase<TSelf>::print(string str) {
    return self().printf("%.*s", str.size, str.data);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(char const *str) {
    return print(cs2s(str));
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(char c) {
    return self().printf("%c", c);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(int8_t val) {
    return self().printf("%hhi", val);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(int16_t val) {
    return self().printf("%hi", val);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(int32_t val) {
    return self().printf("%i", val);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(int64_t val) {
    return self().printf("%lli", val);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(uint8_t val) {
    return self().printf("%hhu", val);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(uint16_t val) {
    return self().printf("%hu", val);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(uint32_t val) {
    return self().printf("%u", val);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(uint64_t val) {
    return self().printf("%llu", val);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(float val) {
    return self().printf("%.*g", std::numeric_limits<float>::max_digits10, val);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(double val) {
    return self().printf("%.*lg", std::numeric_limits<double>::max_digits10, val);
}

template <class TSelf>
int StringBuilderBase<TSelf>::print(void *ptr) {
    return self().printf("%p", ptr);
}

template <class TSelf>
TSelf &StringBuilderBase<TSelf>::self() {
    return *(TSelf *)this;
}

} // namespace nk
