#ifndef HEADER_GUARD_NK_COMMON_HASHSET
#define HEADER_GUARD_NK_COMMON_HASHSET

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>

#include "nk/mem/mem.h"
#include "nk/utils/profiler.hpp"
#include "nk/utils/utils.hpp"

namespace nk {

namespace detail {

template <class T>
struct DefaultHashSetContext {
    static hash_t hash(T const &val) {
        return std::hash<T>{}(val);
    }

    static bool equal_to(T const &lhs, T const &rhs) {
        return std::equal_to<T>{}(lhs, rhs);
    }
};

} // namespace detail

template <class T, class Context = detail::DefaultHashSetContext<T>>
struct HashSet {
    size_t size() const {
        return m_size;
    }

    size_t capacity() const {
        return m_capacity;
    }

    void reserve(size_t cap) {
        _alloc(cap);
    }

    void deinit() {
        nk_platform_free(m_entries);
        *this = {};
    }

    T &insert(T const &val) {
        EASY_BLOCK("HashSet::insert", profiler::colors::Grey200)
        hash_t const hash = _valHash(val);
        _Entry *found = _find(hash, val);
        if (!found) {
            found = _insert(hash, val);
        }
        return (found->val = val);
    }

    template <class U>
    T *find(U const &val) const {
        EASY_BLOCK("HashSet::find", profiler::colors::Grey200)
        _Entry *found = _find(_valHash(val), val);
        return found ? &found->val : nullptr;
    }

    template <class U>
    void remove(U const &val) {
        EASY_BLOCK("HashSet::remove", profiler::colors::Grey200)
        _Entry *found = _find(_valHash(val), val);
        if (found) {
            found->hash |= DELETED_FLAG;
            m_size--;
        }
    }

private:
    struct _Entry {
        T val;
        hash_t hash;

        //@Todo Implement better way to iterate over HashSet
        bool isValid() const {
            return !_isEmpty(this) && !_isDeleted(this);
        }
    };

    static constexpr size_t LOAD_FACTOR_PERCENT = 90;
    static constexpr hash_t DELETED_FLAG = 1ull << (8 * sizeof(hash_t) - 1);

    template <class U>
    static hash_t _valHash(U const &val) {
        hash_t hash = Context::hash(val);
        hash &= ~DELETED_FLAG;
        hash |= hash == 0;
        return hash;
    }

    static bool _isEmpty(_Entry const *entry) {
        return entry->hash == 0;
    }

    static bool _isDeleted(_Entry const *entry) {
        return entry->hash & DELETED_FLAG;
    }

    static bool _isValid(_Entry const *entry) {
        return entry->isValid();
    }

    template <class U>
    static bool _found(_Entry *entry, hash_t hash, U const &val) {
        return _isValid(entry) && entry->hash == hash && Context::equal_to(entry->val, val);
    }

    // capacity must be a power of 2 for quadratic probe sequence with triangular numbers
    size_t _elemIndex(hash_t hash, size_t i) const {
        return (hash + ((i + i * i) >> 1)) & (m_capacity - 1);
    }

    _Entry *_entry(hash_t hash, size_t i) const {
        return &m_entries[_elemIndex(hash, i)];
    }

    void _alloc(size_t cap) {
        static_assert(std::is_trivial_v<_Entry>, "Entry should be trivial");

        m_capacity = ceilToPowerOf2(maxu(cap, 1));
        m_entries = (_Entry *)nk_platform_alloc(m_capacity * sizeof(_Entry));
        std::memset(&m_entries[0], 0, m_capacity * sizeof(_Entry));
    }

    void _rehash() {
        HashSet new_hs{};
        new_hs._alloc(m_capacity << 1);

        for (size_t i = 0; i < m_capacity; i++) {
            _Entry *entry = &m_entries[i];
            if (_isValid(entry)) {
                new_hs._insert(entry->hash, entry->val);
            }
        }

        deinit();
        *this = new_hs;
    }

    _Entry *_insert(hash_t hash, T const &val) {
        if (m_size * 100 / LOAD_FACTOR_PERCENT >= m_capacity) {
            _rehash();
        }

        _Entry *entry = _entry(hash, 0);
        size_t i = 1;
        while (_isValid(entry)) {
            entry = _entry(hash, i++);
        }

        m_max_probe_dist = maxu(m_max_probe_dist, i);

        m_size++;
        entry->val = val;
        entry->hash = hash;

        return entry;
    }

    template <class U>
    _Entry *_find(hash_t hash, U const &val) const {
        for (size_t i = 0; i < m_capacity; i++) {
            _Entry *entry = _entry(hash, i);
            if (_isEmpty(entry) || i > m_max_probe_dist) {
                return nullptr;
            } else if (_found(entry, hash, val)) {
                return entry;
            }
        }
        return nullptr;
    }

public: //@Todo Remove public fields from HashSet
    _Entry *m_entries;

private:
    size_t m_size;
    size_t m_capacity;
    size_t m_max_probe_dist;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_HASHSET
