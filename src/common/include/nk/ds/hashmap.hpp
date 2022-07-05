#ifndef HEADER_GUARD_NK_COMMON_HASHMAP
#define HEADER_GUARD_NK_COMMON_HASHMAP

#include <cstddef>

#include "nk/ds/hashset.hpp"
#include "nk/utils/profiler.hpp"

namespace nk {

namespace detail {

template <class K>
using DefaultHashMapContext = DefaultHashSetContext<K>;

} // namespace detail

template <class K, class V, class Context = detail::DefaultHashMapContext<K>>
struct HashMap {
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

    using _EntrySet = HashSet<_Entry, _HashSetContext>;

public:
    struct iterator : _EntrySet::iterator {
        auto &operator*() {
            return (_TEntry<K const, V> &)_EntrySet::iterator::operator*();
        }
    };

    struct const_iterator : _EntrySet::iterator {
        auto &operator*() {
            return (_TEntry<K const, V const> &)_EntrySet::iterator::operator*();
        }
    };

    iterator begin() {
        return (iterator)m_entries.begin();
    }

    iterator end() {
        return (iterator)m_entries.end();
    }

    const_iterator begin() const {
        return (const_iterator)m_entries.begin();
    }

    const_iterator end() const {
        return (const_iterator)m_entries.end();
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
        EASY_BLOCK("HashMap::insert", profiler::colors::Grey200)
        return m_entries.insert(_Entry{key, value}).value;
    }

    V &operator[](K const &key) {
        EASY_BLOCK("HashMap::operator[]", profiler::colors::Grey200)
        return m_entries.insert(_Entry{key, {}}).value;
    }

    V *find(K const &key) const {
        EASY_BLOCK("HashMap::find", profiler::colors::Grey200)
        _Entry *found = m_entries.find(key);
        return found ? &found->value : nullptr;
    }

    void remove(K const &key) {
        EASY_BLOCK("HashMap::remove", profiler::colors::Grey200)
        m_entries.remove(key);
    }

private:
    _EntrySet m_entries;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_HASHMAP
