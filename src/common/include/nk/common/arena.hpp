#ifndef HEADER_GUARD_NK_COMMON_ARENA
#define HEADER_GUARD_NK_COMMON_ARENA

#include <cstddef>
#include <cstdint>

#include "nk/common/mem.hpp"
#include "nk/common/sequence.hpp"

struct ArenaAllocator : SeqAllocator {
    void *alloc(size_t size) override;
    void free(void *ptr) override;

    size_t size() const override;
    void pop(size_t n) override;

    using Allocator::alloc;

    static ArenaAllocator create(size_t cap = c_init_capacity);
    void init(size_t cap = c_init_capacity);
    void deinit();
    void clear();

    static constexpr size_t c_init_capacity = 4096;

    Sequence<uint8_t> _seq;
};

#endif // HEADER_GUARD_NK_COMMON_ARENA
