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
    };

    Array<Entry> entries;
    size_t size;
    size_t capacity;

    void reserve(size_t cap) {
        _alloc(cap);
    }

    void deinit() {
        entries.deinit();
        *this = {};
    }

    bool insert(K const &key, V const &value) {
        EASY_BLOCK("HashMap::insert", profiler::colors::Grey200)
        return _insert(_keyHash(key), key, value);
    }

    V *find(K const &key) const {
        EASY_BLOCK("HashMap::find", profiler::colors::Grey200)
        Entry *entry = _find(key);
        return entry ? &entry->value : nullptr;
    }

    void remove(K const &key) {
        EASY_BLOCK("HashMap::remove", profiler::colors::Grey200)
        Entry *entry = _find(key);
        if (entry) {
            entry->hash |= DELETED_FLAG;
            size--;
        }
        return;
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
        return !_isEmpty(entry) && !_isDeleted(entry);
    }

    static bool _found(Entry *entry, hash_t hash, K const &key) {
        return _isValid(entry) && entry->hash == hash && Context::equal_to(entry->key, key);
    }

    // capacity must be a power of 2 for quadratic probe sequence with triangular numbers
    size_t _elemIndex(hash_t hash, size_t i) const {
        return (hash + ((i) + (i) * (i)) / 2) % capacity;
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
                new_hm._insert(entry->hash, entry->key, entry->value);
            }
        }

        deinit();
        *this = new_hm;
    }

    bool _insert(hash_t hash, K const &key, V const &value) {
        if (size * 100 / LOAD_FACTOR_PERCENT >= capacity) {
            _rehash();
        }

        size_t probe_length = 0;
        Entry *entry;

        bool cleanup = false;
        bool res = false;
        do {
            entry = &entries[_elemIndex(hash, probe_length)];

            if (cleanup) {
                if (_found(entry, hash, key)) {
                    size--;
                    entry->hash |= DELETED_FLAG;
                    break;
                }
            } else if (_isDeleted(entry)) {
                size++;
                res = true;
                *entry = {
                    .key = key,
                    .value = value,
                    .hash = hash,
                };
                cleanup = true;
            } else if (_isEmpty(entry) || _found(entry, hash, key)) {
                if (!_isValid(entry)) {
                    size++;
                    res = true;
                }
                *entry = {
                    .key = key,
                    .value = value,
                    .hash = hash,
                };
                break;
            }

            probe_length++;
        } while (!_isEmpty(entry) && probe_length < capacity);

        return res;
    }

    Entry *_find(K const &key) const {
        hash_t hash = _keyHash(key);

        size_t probe_length = 0;
        Entry *entry;

        while (probe_length < capacity) {
            entry = &entries[_elemIndex(hash, probe_length++)];
            if (_isEmpty(entry)) {
                break;
            } else if (_found(entry, hash, key)) {
                return entry;
            }
        }

        return nullptr;
    }
};

#endif // HEADER_GUARD_NK_COMMON_HASHMAP
