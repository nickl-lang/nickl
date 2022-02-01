#ifndef HEADER_GUARD_NK_COMMON_HASHMAP
#define HEADER_GUARD_NK_COMMON_HASHMAP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>

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

}; // namespace detail

template <class K, class V, class Context = detail::DefaultHashMapContext<K>>
struct HashMap {
    struct Entry {
        K key;
        V value;
        hash_t hash;
        bool deleted;
    };

    Array<Entry> entries;
    size_t size;
    size_t capacity;

    static HashMap create(size_t cap = 0) {
        HashMap hm;
        hm.init(cap);
        return hm;
    }

    void init(size_t cap = 0) {
        size = 0;
        capacity = ceilToPowerOf2(cap ? cap : INIT_CAPACITY);

        _alloc();
    }

    void deinit() {
        entries.deinit();
        size = 0;
        capacity = 0;
    }

    V &insert(K const &key) {
        EASY_BLOCK("HashMap::insert", profiler::colors::Grey200)
        return *_insert(_keyHash(key), key);
    }

    V *find(K const &key) {
        EASY_BLOCK("HashMap::find", profiler::colors::Grey200)
        Entry *entry = _find(key);
        return entry ? &entry->value : nullptr;
    }

    void remove(K const &key) {
        EASY_BLOCK("HashMap::remove", profiler::colors::Grey200)
        Entry *entry = _find(key);
        if (entry) {
            entry->deleted = true;
            size--;
        }
        return;
    }

private:
    static constexpr size_t INIT_CAPACITY = 1;
    static constexpr double LOAD_FACTOR = 0.5;

    static hash_t _keyHash(K const &key) {
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
    size_t _elemIndex(hash_t hash, size_t i) {
        return (_hash(hash) + ((i) + (i) * (i)) / 2) % capacity;
    }

    void _alloc() {
        entries.init(capacity);
        std::fill(entries.data, entries.data + entries.capacity, Entry{});
    }

    void _rehash() {
        HashMap new_hm;

        new_hm.size = 0;
        new_hm.capacity = capacity << 1;

        new_hm._alloc();

        for (size_t i = 0; i < capacity; i++) {
            Entry *entry = &entries[i];
            if (_isValid(entry)) {
                *new_hm._insert(entry->hash, entry->key) = entry->value;
            }
        }

        deinit();
        *this = new_hm;
    }

    V *_insert(hash_t hash, K const &key) {
        if (size >= capacity * LOAD_FACTOR) {
            _rehash();
        }

        size_t probe_length = 0;
        Entry *entry;

        bool cleanup = false;
        Entry *ret_entry = nullptr;

        do {
            size_t i = _elemIndex(hash, probe_length);
            entry = &entries[i];

            if (cleanup) {
                if (_found(entry, hash, key)) {
                    size--;
                    entry->deleted = true;
                    break;
                }
            } else if (_isDeleted(entry)) {
                ret_entry = entry;
                cleanup = true;
            } else if (_isEmpty(entry) || _found(entry, hash, key)) {
                ret_entry = entry;
                break;
            }

            probe_length++;
        } while (!_isEmpty(entry) && probe_length <= capacity);

        assert(ret_entry && "entry not found to insert");

        if (!_isValid(ret_entry)) {
            size++;
        }

        ret_entry->key = key;
        ret_entry->hash = hash;
        ret_entry->deleted = false;

        return &ret_entry->value;
    }

    Entry *_find(K const &key) {
        hash_t hash = _keyHash(key);

        size_t probe_length = 0;
        Entry *entry;

        do {
            size_t i = _elemIndex(hash, probe_length);
            entry = &entries[i];

            if (_found(entry, hash, key)) {
                return entry;
            }

            probe_length++;
        } while (!_isEmpty(entry) && probe_length <= capacity);

        return nullptr;
    }
};

#endif // HEADER_GUARD_NK_COMMON_HASHMAP
