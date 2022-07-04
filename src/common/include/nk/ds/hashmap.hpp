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

    HashSet<_Entry, _HashSetContext> entries;

    void reserve(size_t cap) {
        entries.reserve(cap);
    }

    void deinit() {
        entries.deinit();
    }

    size_t size() const {
        return entries.size;
    }

    size_t capacity() const {
        return entries.capacity;
    }

    bool insert(K const &key, V const &value) {
        EASY_BLOCK("HashMap::insert", profiler::colors::Grey200)
        //@Robustness Creating a default value for find in HashMap
        _Entry *found = entries.find(_Entry{key, {}});
        if (found) {
            found->value = value;
        } else {
            entries.insert(_Entry{key, value});
        }
        return !found;
    }

    V &operator[](K const &key) {
        EASY_BLOCK("HashMap::operator[]", profiler::colors::Grey200)
        _Entry *found = entries.find(_Entry{key, {}});
        if (!found) {
            entries.insert(_Entry{key, {}});
            //@Robustness Finding again in HashMap::operator[]
            found = entries.find(_Entry{key, {}});
        }
        return found->value;
    }

    V *find(K const &key) const {
        EASY_BLOCK("HashMap::find", profiler::colors::Grey200)
        _Entry *found = entries.find(_Entry{key, {}});
        return found ? &found->value : nullptr;
    }

    void remove(K const &key) {
        EASY_BLOCK("HashMap::remove", profiler::colors::Grey200)
        entries.remove(_Entry{key, {}});
    }
};

} // namespace nk

#endif // HEADER_GUARD_NK_COMMON_HASHMAP
