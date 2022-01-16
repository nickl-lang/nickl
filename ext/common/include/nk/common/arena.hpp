#ifndef HEADER_GUARD_NK_COMMON_ARENA
#define HEADER_GUARD_NK_COMMON_ARENA

#include <cstddef>
#include <cstdint>

#include "nk/common/mem.hpp"
#include "nk/common/sequence.hpp"

struct ArenaAllocator : Allocator {
    void *alloc(size_t size) override;
    void free(void *ptr) override;

    using Allocator::alloc;

    static ArenaAllocator create(size_t cap = c_init_capacity);
    void init(size_t cap = c_init_capacity);
    void deinit();
    void clear();
    size_t size() const;

    static constexpr size_t c_init_capacity = 4096;

    Sequence<uint8_t> _seq;
};

#endif // HEADER_GUARD_NK_COMMON_ARENA
