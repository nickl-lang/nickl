#ifndef NTK_HASH_SET_H_
#define NTK_HASH_SET_H_

#include <cstring>
#include <functional>
#include <type_traits>

#include "ntk/allocator.h"
#include "ntk/profiler.h"
#include "ntk/utils.h"

template <class T>
struct NkHashSetDefaultContext {
    static u64 hash(T const &val) {
        return std::hash<T>{}(val);
    }

    static bool equal_to(T const &lhs, T const &rhs) {
        return std::equal_to<T>{}(lhs, rhs);
    }
};

template <class T, class Context = NkHashSetDefaultContext<T>>
struct NkHashSet {
private:
    struct _Entry {
        T val;
        u64 hash;
    };

public:
    static NkHashSet create() {
        return create({});
    }

    static NkHashSet create(NkAllocator alloc) {
        NkHashSet set{};
        set.m_alloc = alloc;
        return set;
    }

    template <class TValue>
    struct TIterator {
        _Entry *_entry;
        _Entry *_end;

        TIterator &operator++() {
            _entry++;
            while (_entry != _end && !_isValid(_entry)) {
                _entry++;
            }
            return *this;
        }

        TIterator operator++(int) {
            TIterator tmp{*this};
            operator++();
            return tmp;
        }

        bool operator!=(TIterator const &rhs) {
            return _entry != rhs._entry;
        }

        TValue &operator*() {
            return (TValue &)_entry->val;
        }

        template <class U>
        operator TIterator<U>() {
            return {_entry, _end};
        }
    };

    using iterator = TIterator<T const>;

    iterator begin() const {
        iterator it{m_entries - 1, m_entries + m_capacity};
        return ++it;
    }

    iterator end() const {
        return {m_entries + m_capacity, m_entries + m_capacity};
    }

    usize size() const {
        return m_size;
    }

    usize capacity() const {
        return m_capacity;
    }

    void reserve(usize cap) {
        _allocate(cap);
    }

    void deinit() {
        nk_freeT(alloc(), m_entries, m_capacity);

        m_entries = nullptr;
        m_size = 0;
        m_capacity = 0;
        m_max_probe_dist = 0;
    }

    T &insert(T const &val) {
        NK_PROF_SCOPE(nk_cs2s("NkHashSet::insert"));
        u64 const hash = _valHash(val);
        _Entry *found = _find(hash, val);
        if (found) {
            found->val = val;
        } else {
            found = _insert(hash, val);
        }
        return found->val;
    }

    template <class U>
    T *find(U const &val) const {
        NK_PROF_SCOPE(nk_cs2s("NkHashSet::find"));
        _Entry *found = _find(_valHash(val), val);
        return found ? &found->val : nullptr;
    }

    template <class U>
    void remove(U const &val) {
        NK_PROF_SCOPE(nk_cs2s("NkHashSet::remove"));
        _Entry *found = _find(_valHash(val), val);
        if (found) {
            found->hash |= DELETED_FLAG;
            m_size--;
        }
    }

private:
    static constexpr usize LOAD_FACTOR_PERCENT = 90;
    static constexpr u64 DELETED_FLAG = 1ull << (8 * sizeof(u64) - 1);

    template <class U>
    static u64 _valHash(U const &val) {
        u64 hash = Context::hash(val);
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
        return !_isEmpty(entry) && !_isDeleted(entry);
    }

    template <class U>
    static bool _found(_Entry *entry, u64 hash, U const &val) {
        return _isValid(entry) && entry->hash == hash && Context::equal_to(entry->val, val);
    }

    // capacity must be a power of 2 for quadratic probe sequence with triangular numbers
    usize _elemIndex(u64 hash, usize i) const {
        return (hash + ((i + i * i) >> 1)) & (m_capacity - 1);
    }

    _Entry *_entry(u64 hash, usize i) const {
        return &m_entries[_elemIndex(hash, i)];
    }

    void _allocate(usize cap) {
        static_assert(std::is_trivial_v<_Entry>, "Entry should be trivial");

        m_capacity = nk_ceilToPowerOf2(nk_maxu(cap, 1));
        m_entries = nk_allocT<_Entry>(alloc(), m_capacity);
        std::memset(&m_entries[0], 0, m_capacity * sizeof(_Entry));
    }

    void _rehash() {
        auto new_hs = create(m_alloc);
        new_hs._allocate(m_capacity << 1);

        for (usize i = 0; i < m_capacity; i++) {
            _Entry *entry = &m_entries[i];
            if (_isValid(entry)) {
                new_hs._insert(entry->hash, entry->val);
            }
        }

        deinit();

        m_entries = new_hs.m_entries;
        m_size = new_hs.m_size;
        m_capacity = new_hs.m_capacity;
        m_max_probe_dist = new_hs.m_max_probe_dist;
    }

    _Entry *_insert(u64 hash, T const &val) {
        if (m_size * 100 / LOAD_FACTOR_PERCENT >= m_capacity) {
            _rehash();
        }

        _Entry *entry = _entry(hash, 0);
        usize i = 1;
        while (_isValid(entry)) {
            entry = _entry(hash, i++);
        }

        m_max_probe_dist = nk_maxu(m_max_probe_dist, i);

        m_size++;
        entry->val = val;
        entry->hash = hash;

        return entry;
    }

    template <class U>
    _Entry *_find(u64 hash, U const &val) const {
        for (usize i = 0; i < m_capacity; i++) {
            _Entry *entry = _entry(hash, i);
            if (_isEmpty(entry) || i > m_max_probe_dist) {
                return nullptr;
            } else if (_found(entry, hash, val)) {
                return entry;
            }
        }
        return nullptr;
    }

    NkAllocator alloc() const {
        if (m_alloc.proc) {
            return m_alloc;
        } else {
            return nk_default_allocator;
        }
    }

private:
    NkAllocator m_alloc;
    _Entry *m_entries;
    usize m_size;
    usize m_capacity;
    usize m_max_probe_dist;
};

#endif // NTK_HASH_SET_H_
