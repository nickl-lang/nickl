#include "nk/common/arena.hpp"

#include "nk/common/logger.hpp"

void *ArenaAllocator::alloc(size_t size) {
    void *res = &_seq.push(size);
    return res;
}

void ArenaAllocator::free(void *ptr) {
    (void)ptr;
}

ArenaAllocator ArenaAllocator::create(size_t cap) {
    ArenaAllocator ar;
    ar.init(cap);
    return ar;
}

void ArenaAllocator::init(size_t cap) {
    _seq.init(cap);
}

void ArenaAllocator::deinit() {
    _seq.deinit();
}

void ArenaAllocator::clear() {
    _seq.clear();
}

size_t ArenaAllocator::size() const {
    return _seq.size;
}
