#ifndef HEADER_GUARD_NK_COMMON_HASHMAP
#define HEADER_GUARD_NK_COMMON_HASHMAP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>

#include "nk/common/array.hpp"
#include "nk/common/profiler.hpp"
#include "nk/common/utils.hpp"

namespace nk {

namespace detail {

template <class K>
struct DefaultHashMapContext {
    static hash_t hash(K const &key) {
        return std::hash<K>{}(key);
    }

    static bool equal_to(K const &lhs, K const &rhs) {
        return std::equal_to<K>{}(lhs, rhs);
    }
};

} // namespace detail

template <class K, class V, class Context = detail::DefaultHashMapContext<K>>
struct HashMap {
    struct Entry {
        K key;
        V value;
        hash_t hash;

        //@Todo Implement better way to iterate over HashMap
        bool isValid() const {
            return !_isEmpty(this) && !_isDeleted(this);
        }
    };

    Array<Entry> entries;
    size_t size;
    size_t capacity;
    size_t max_probe_dist;

    void reserve(size_t cap) {
        _alloc(cap);
    }

    void deinit() {
        entries.deinit();
        *this = {};
    }

    bool insert(K const &key, V const &value) {
        EASY_BLOCK("HashMap::insert", profiler::colors::Grey200)
        hash_t const hash = _keyHash(key);
        Entry *found = _find(hash, key);
        if (found) {
            found->value = value;
        } else {
            _insert(hash, key)->value = value;
        }
        return !found;
    }

    V &operator[](K const &key) {
        EASY_BLOCK("HashMap::operator[]", profiler::colors::Grey200)
        hash_t const hash = _keyHash(key);
        Entry *found = _find(hash, key);
        if (!found) {
            (found = _insert(hash, key))->value = V{};
        }
        return found->value;
    }

    V *find(K const &key) const {
        EASY_BLOCK("HashMap::find", profiler::colors::Grey200)
        Entry *found = _find(_keyHash(key), key);
        return found ? &found->value : nullptr;
    }

    void remove(K const &key) {
        EASY_BLOCK("HashMap::remove", profiler::colors::Grey200)
        Entry *found = _find(_keyHash(key), key);
        if (found) {
            found->hash |= DELETED_FLAG;
            size--;
        }
    }

private:
    static constexpr size_t LOAD_FACTOR_PERCENT = 90;
    static constexpr hash_t DELETED_FLAG = 1ull << (8 * sizeof(hash_t) - 1);

    static hash_t _keyHash(K const &key) {
        hash_t hash = Context::hash(key);
        hash &= ~DELETED_FLAG;
        hash |= hash == 0;
        return hash;
    }

    static bool _isEmpty(Entry const *entry) {
        return entry->hash == 0;
    }

    static bool _isDeleted(Entry const *entry) {
        return entry->hash & DELETED_FLAG;
    }

    static bool _isValid(Entry const *entry) {
        return entry->isValid();
    }

    static bool _found(Entry *entry, hash_t hash, K const &key) {
        return _isValid(entry) && entry->hash == hash && Context::equal_to(entry->key, key);
    }

    // capacity must be a power of 2 for quadratic probe sequence with triangular numbers
    size_t _elemIndex(hash_t hash, size_t i) const {
        return (hash + ((i + i * i) >> 1)) & (capacity - 1);
    }

    Entry *_entry(hash_t hash, size_t i) const {
        return &entries[_elemIndex(hash, i)];
    }

    void _alloc(size_t cap) {
        static_assert(std::is_trivial_v<Entry>, "Entry should be trivial");

        capacity = ceilToPowerOf2(maxu(cap, 1));
        entries.reserve(capacity);
        std::memset(&entries[0], 0, capacity * sizeof(Entry));
    }

    void _rehash() {
        HashMap new_hm{};
        new_hm._alloc(capacity << 1);

        for (size_t i = 0; i < capacity; i++) {
            Entry *entry = &entries[i];
            if (_isValid(entry)) {
                new_hm._insert(entry->hash, entry->key)->value = entry->value;
            }
        }

        deinit();
        *this = new_hm;
    }

    Entry *_insert(hash_t hash, K const &key) {
        if (size * 100 / LOAD_FACTOR_PERCENT >= capacity) {
            _rehash();
        }

        Entry *entry = _entry(hash, 0);
        size_t i = 1;
        while (_isValid(entry)) {
            entry = _entry(hash, i++);
        }

        max_probe_dist = maxu(max_probe_dist, i);

        size++;
        entry->key = key;
        entry->hash = hash;

        return entry;
    }

    Entry *_find(hash_t hash, K const &key) const {
        for (size_t i = 0; i < capacity; i++) {
            Entry *entry = _entry(hash, i);
            if (_isEmpty(entry) || i > max_probe_dist) {
                return nullptr;
            } else if (_found(entry, hash, key)) {
                return entry;
            }
        }
        return nullptr;
    }
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_HASHMAP
