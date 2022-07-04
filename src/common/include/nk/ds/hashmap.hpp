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
        //@Robustness Creating a default value for find in HashMap
        _Entry *found = m_entries.find(_Entry{key, {}});
        if (!found) {
            found = &m_entries.insert(_Entry{key, {}});
        }
        return found->value;
    }

    V *find(K const &key) const {
        EASY_BLOCK("HashMap::find", profiler::colors::Grey200)
        //@Robustness Creating a default value for find in HashMap
        _Entry *found = m_entries.find(_Entry{key, {}});
        return found ? &found->value : nullptr;
    }

    void remove(K const &key) {
        EASY_BLOCK("HashMap::remove", profiler::colors::Grey200)
        //@Robustness Creating a default value for find in HashMap
        m_entries.remove(_Entry{key, {}});
    }

private:
    struct _Entry {
        K key;
        V value;
    };

    struct _HashSetContext {
        static hash_t hash(_Entry const &entry) {
            return Context::hash(entry.key);
        }

        static bool equal_to(_Entry const &lhs, _Entry const &rhs) {
            return Context::equal_to(lhs.key, rhs.key);
        }
    };

    HashSet<_Entry, _HashSetContext> m_entries;
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_HASHMAP
