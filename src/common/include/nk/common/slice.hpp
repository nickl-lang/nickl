#ifndef HEADER_GUARD_NK_COMMON_SLICE
#define HEADER_GUARD_NK_COMMON_SLICE

#include <cstddef>
#include <iterator>
#include <memory>

template <class T>
struct nkslice {
    using value_type = T;
    using pointer = value_type *;
    using iterator = pointer;
    using reference = value_type &;
    using size_type = size_t;

    pointer _data;
    size_type _size;

    constexpr nkslice()
        : _data{}
        , _size{} {
    }

    template <class TContainer>
    constexpr nkslice(TContainer &cont)
        : _data{std::addressof(*std::begin(cont))}
        , _size{cont.size()} {
    }

    constexpr nkslice(pointer data, size_type size)
        : _data{data}
        , _size{size} {
    }

    constexpr iterator data() const {
        return _data;
    }

    constexpr size_type size() const {
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

    constexpr reference operator*() const {
        return _data[0];
    }

    constexpr iterator begin() const noexcept {
        return _data;
    }

    constexpr iterator end() const noexcept {
        return _data + _size;
    }

    constexpr operator nkslice<T const>() const {
        return {_data, _size};
    }
};

#endif // HEADER_GUARD_NK_COMMON_SLICE
