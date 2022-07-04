#ifndef HEADER_GUARD_NK_COMMON_DYNAMIC_STRING_BUILDER
#define HEADER_GUARD_NK_COMMON_DYNAMIC_STRING_BUILDER

#include "nk/common/arena.hpp"
#include "nk/common/string_builder.hpp"

namespace nk {

struct DynamicStringBuilder : StringBuilder {
public:
    void reserve(size_t n);

    size_t size() const override {
        return m_arena.size;
    }

    int printf(char const *fmt, ...) override;

    using StringBuilder::moveStr;
    string moveStr(Slice<char> dst) override;

private:
    Arena<char> m_arena;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_DYNAMIC_STRING_BUILDER
