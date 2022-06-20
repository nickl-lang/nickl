#ifndef HEADER_GUARD_NK_COMMON_HASHMAP
#define HEADER_GUARD_NK_COMMON_HASHMAP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>

#include "nk/common/array.hpp"
#include "nk/common/logger.h" // TODO REMOVE
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
    constexpr LOG_USE_SCOPE(hashmap_debug); // TODO REMOVE

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
        LOG_DBG("##################################### insert key=%i", key); // TODO REMOVE
        defer {
            printState();
        };

        EASY_BLOCK("HashMap::insert", profiler::colors::Grey200)
        return _insert(_hashKey(key), key, value);
    }

    V *find(K const &key) const {
        EASY_BLOCK("HashMap::find", profiler::colors::Grey200)
        Entry *entry = _find(key);
        return entry ? &entry->value : nullptr;
    }

    bool remove(K const &key) {
        LOG_DBG("##################################### remove key=%i", key); // TODO REMOVE
        defer {
            printState();
        };

        EASY_BLOCK("HashMap::remove", profiler::colors::Grey200)
        Entry *entry = _find(key);
        if (entry) {
            entry->hash |= DELETED_FLAG;
            size--;
            return true;
        }
        return false;
    }

private:
    static constexpr size_t LOAD_FACTOR_PERCENT = 100;
    static constexpr hash_t DELETED_FLAG = 1ul << (8 * sizeof(hash_t) - 1);

    static hash_t _hashKey(K const &key) {
        hash_t h = Context::hash(key);
        h &= ~DELETED_FLAG;
        h |= h == 0;
        return h;
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

    static bool _found(Entry const *entry, hash_t hash, K const &key) {
        return _isValid(entry) && entry->hash == hash && Context::equal_to(entry->key, key);
    }

    size_t _probeDistance(Entry const *entry, size_t slot_idx) const {
        return (slot_idx + capacity - (entry->hash & (capacity - 1))) & (capacity - 1);
    }

    void _alloc(size_t cap) {
        static_assert(std::is_trivial_v<Entry>, "Entry should be trivial");

        capacity = ceilToPowerOf2(maxu(cap, 1));
        entries.reserve(capacity);
        std::memset(entries.begin(), 0, capacity * sizeof(Entry));
    }

    void _rehash() {
        LOG_DBG("///////////////////////////////////// rehash"); // TODO REMOVE

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

        LOG_DBG("///////////////////////////////////// rehash DONE"); // TODO REMOVE
    }

    bool _insert(hash_t _hash, K const &_key, V const &_value) {
        if (size * 100 / LOAD_FACTOR_PERCENT >= capacity) {
            _rehash();
        }

        LOG_DBG("size=%zu", size);         // TODO REMOVE
        LOG_DBG("capacity=%zu", capacity); // TODO REMOVE

        Entry new_entry{
            .key = _key,
            .value = _value,
            .hash = _hash,
        };

        Entry *entry = nullptr;
        size_t dist = 0;
        size_t pos = new_entry.hash & (capacity - 1);

        for (;;) {
            entry = &entries[pos];

            LOG_DBG("========================== OK pos=%zu", pos); // TODO REMOVE

            if (_isEmpty(entry) || _found(entry, new_entry.hash, new_entry.key)) {
                break;
            }

            LOG_DBG("it's not empty and not found"); // TODO REMOVE

            size_t existing_dist = _probeDistance(entry, pos);
            LOG_DBG("dist=%zu", dist);                   // TODO REMOVE
            LOG_DBG("existing_dist=%zu", existing_dist); // TODO REMOVE
            if (existing_dist < dist) {
                if (_isDeleted(entry)) {
                    break;
                }
                LOG_DBG("it's not deleted, swapping"); // TODO REMOVE
                std::swap(new_entry, *entry);
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

        LOG_DBG("Found!!!! inserting");

        *entry = new_entry;

        return inserted;
    }

    void printState() { // TODO REMOVE
        fprintf(stderr, "STATE [\n");
        for (size_t idx = 0; idx < capacity; idx++) {
            auto &entry = entries[idx];
            fprintf(stderr, "(idx=%zu", idx);
            if (_isEmpty(&entry)) {
                fprintf(stderr, " // EMPTY //");
            } else if (_isDeleted(&entry)) {
                fprintf(stderr, " // DELETED hash=%zu //", entry.hash);
            } else if (_isValid(&entry)) {
                fprintf(stderr, " hash=%zu key=%i value=%i", entry.hash, entry.key, entry.value);
            }
            fprintf(stderr, ")\n");
        }
        fprintf(stderr, "]\n");
        fflush(stderr);
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
            } else if (dist > _probeDistance(entry, pos)) {
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
