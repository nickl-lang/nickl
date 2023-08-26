#ifndef HEADER_GUARD_NK_COMMON_HASH_MAP
#define HEADER_GUARD_NK_COMMON_HASH_MAP

#include "nk/common/hash_set.hpp"

template <class K, class V, class Context = NkHashSetDefaultCountext<K>>
struct NkHashMap {
private:
    template <class TKey, class TValue>
    struct _TEntry {
        TKey key;
        TValue value;
    };

    using _Entry = _TEntry<K, V>;

    struct _HashSetContext {
        static hash_t hash(_Entry const &entry) {
            return Context::hash(entry.key);
        }

        static hash_t hash(K const &key) {
            return Context::hash(key);
        }

        static bool equal_to(_Entry const &lhs, _Entry const &rhs) {
            return Context::equal_to(lhs.key, rhs.key);
        }

        static bool equal_to(_Entry const &lhs, K const &rhs_key) {
            return Context::equal_to(lhs.key, rhs_key);
        }
    };

    using _EntrySet = NkHashSet<_Entry, _HashSetContext>;

public:
    NkHashMap() = default;
    NkHashMap(NkAllocator alloc)
        : m_entries{alloc} {
    }

    using iterator = typename _EntrySet::template TIterator<_TEntry<K const, V>>;
    using const_iterator = typename _EntrySet::template TIterator<_TEntry<K const, V const>>;

    iterator begin() {
        return m_entries.begin();
    }

    iterator end() {
        return m_entries.end();
    }

    const_iterator begin() const {
        return m_entries.begin();
    }

    const_iterator end() const {
        return m_entries.end();
    }

    void reserve(size_t cap) {
        m_entries.reserve(cap);
    }

    void deinit() {
        m_entries.deinit();
    }

    size_t size() const {
        return m_entries.size();
    }

    size_t capacity() const {
        return m_entries.capacity();
    }

    V &insert(K const &key, V const &value) {
        return m_entries.insert(_Entry{key, value}).value;
    }

    V &operator[](K const &key) {
        return m_entries.insert(_Entry{key, {}}).value;
    }

    V *find(K const &key) const {
        _Entry *found = m_entries.find(key);
        return found ? &found->value : nullptr;
    }

    void remove(K const &key) {
        m_entries.remove(key);
    }

private:
    _EntrySet m_entries;
};

#endif // HEADER_GUARD_NK_COMMON_HASH_MAP
