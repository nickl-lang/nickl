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
        bool deleted; // TODO flag deleted items with hash
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
        return _insert(_hashKey(key), key, value);
    }

    V *find(K const &key) const {
        EASY_BLOCK("HashMap::find", profiler::colors::Grey200)
        Entry *entry = _find(key);
        return entry ? &entry->value : nullptr;
    }

    bool remove(K const &key) {
        EASY_BLOCK("HashMap::remove", profiler::colors::Grey200)
        Entry *entry = _find(key);
        if (entry) {
            entry->deleted = true;
            size--;
            return true;
        }
        return false;
    }

private:
    static constexpr size_t INIT_CAPACITY = 1;
    static constexpr double LOAD_FACTOR = 0.5;

    static hash_t _hashKey(K const &key) {
        return _hash(Context::hash(key));
    }

    static hash_t _hash(hash_t hash) {
        return hash | 1;
    }

    static bool _isEmpty(Entry const *entry) {
        return entry->hash == 0;
    }

    static bool _isDeleted(Entry const *entry) {
        return entry->deleted;
    }

    static bool _isValid(Entry const *entry) {
        return !_isEmpty(entry) && !_isDeleted(entry);
    }

    static bool _found(Entry *entry, hash_t hash, K const &key) {
        return _isValid(entry) && _hash(entry->hash) == _hash(hash) &&
               Context::equal_to(entry->key, key);
    }

    // capacity must be a power of 2 for quadratic probe sequence with triangular numbers
    size_t _elemIndex(hash_t hash, size_t i) const {
        return (_hash(hash) + ((i) + (i) * (i)) / 2) & (capacity - 1);
    }

    void _alloc(size_t cap) {
        static_assert(std::is_trivial_v<Entry>, "Entry should be trivial");

        capacity = ceilToPowerOf2(maxu(cap, 1));
        entries.reserve(capacity);
        std::memset(entries.begin(), 0, capacity * sizeof(Entry));
    }

    void _rehash() {
        HashMap new_hm{};
        new_hm._alloc(maxu(capacity, 1) << 1);

        for (size_t i = 0; i < capacity; i++) {
            Entry *entry = &entries[i];
            if (_isValid(entry)) {
                new_hm._insert(entry->hash, entry->key, entry->value);
            }
        }

        deinit();
        *this = new_hm;
    }

    size_t _probeDistance(hash_t hash, size_t slot_idx) const {
        return (slot_idx + capacity - (hash & (capacity - 1))) & (capacity - 1);
    }

    bool _insert(hash_t hash, K key, V value) {
        if (size >= capacity * LOAD_FACTOR) {
            _rehash();
        }

        Entry *entry = nullptr;
        size_t dist = 0;
        size_t pos = hash & (capacity - 1);

        for (;;) {
            entry = &entries[pos];

            if (_isEmpty(entry) || _found(entry, hash, key)) {
                break;
            }

            size_t existing_dist = _probeDistance(entry->hash, pos);
            if (existing_dist < dist) {
                if (_isDeleted(entry)) {
                    break;
                }
                std::swap(hash, entry->hash);
                std::swap(key, entry->key);
                std::swap(value, entry->value);
                dist = existing_dist;
            }

            pos = (pos + 1) & (capacity - 1);
            dist++;
        }

        bool inserted = false;
        if (!_isValid(entry)) {
            size++;
            inserted = true;
        }

        *entry = {
            .key = key,
            .value = value,
            .hash = hash,
            .deleted = false,
        };

        return inserted;
    }

    Entry *_find(K const &key) const {
        if (!capacity) {
            return nullptr;
        }
        hash_t const hash = _hashKey(key);
        size_t pos = hash & (capacity - 1);
        size_t dist = 0;
        for (;;) {
            auto entry = &entries[pos];
            if (_isEmpty(entry)) {
                return nullptr;
            } else if (dist > _probeDistance(entry->hash, pos)) {
                return nullptr;
            } else if (_found(entry, hash, key)) {
                return entry;
            }
            pos = (pos + 1) & (capacity - 1);
            dist++;
        }
    }
};

#endif // HEADER_GUARD_NK_COMMON_HASHMAP
