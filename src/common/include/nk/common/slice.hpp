#ifndef HEADER_GUARD_NK_COMMON_SLICE
#define HEADER_GUARD_NK_COMMON_SLICE

#include <cstddef>
#include <iterator>
#include <memory>

template <class T>
struct nkslice {
    T *_data;
    size_t _size;

    using value_type = T;
    using iterator = T *;
    using reference = value_type &;
    using size_type = size_t;

    constexpr nkslice()
        : _data{}
        , _size{} {
    }

    template <class TContainer>
    constexpr nkslice(TContainer &cont)
        : _data{std::addressof(*std::begin(cont))}
        , _size{cont.size()} {
    }

    constexpr iterator data() const {
        return _data;
    }

    constexpr size_t size() const {
        return _size;
    }

    constexpr reference at(size_type pos) const {
        return _data[pos];
    }

    constexpr reference operator[](size_type pos) const {
        return _data[pos];
    }

    constexpr reference front() const {
        return _data[0];
    }

    constexpr reference back() const {
        return _data[_size - 1];
    }

    constexpr iterator begin() const noexcept {
        return _data;
    }

    constexpr iterator end() const noexcept {
        return _data + _size;
    }

    operator nkslice<T const>() const {
        return {_data, _size};
    }
};

#endif // HEADER_GUARD_NK_COMMON_SLICE
