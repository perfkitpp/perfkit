//
// Created by Seungwoo on 2021-08-26.
//
#pragma once
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <type_traits>

namespace KANGSW_ARRAY_VIEW_NAMESPACE {

template <typename Ty_>
class array_view {
 public:
  using value_type    = Ty_;
  using pointer       = value_type*;
  using const_pointer = value_type const*;
  using reference     = value_type&;

 public:
  constexpr array_view() noexcept = default;
  constexpr array_view(Ty_* p, size_t n) noexcept
      : _ptr(p), _size(n) {}

  template <typename Range_>
  constexpr array_view(Range_&& p) noexcept
      : array_view(p.data(), p.size()) {}

  constexpr auto size() const noexcept { return _size; }
  constexpr auto data() const noexcept { return _ptr; }
  constexpr auto data() noexcept { return _ptr; }

  constexpr auto begin() noexcept { return _ptr; }
  constexpr auto begin() const noexcept { return _ptr; }
  constexpr auto end() noexcept { return _ptr + _size; }
  constexpr auto end() const noexcept { return _ptr + _size; }

  constexpr auto& front() const noexcept { return at(0); }
  constexpr auto& back() const noexcept { return at(_size - 1); }

  constexpr auto empty() const noexcept { return size() == 0; }

  constexpr auto subspan(size_t offset, size_t n = ~size_t{}) const {
    if (offset == _size) { return array_view{_ptr, 0}; }
    _verify_idx(offset);

    return array_view{_ptr + offset, std::min(n, _size - offset)};
  }

  constexpr auto& operator[](size_t idx) {
#ifndef NDEBUG
    _verify_idx(idx);
#endif
    return _ptr[idx];
  };

  constexpr auto& operator[](size_t idx) const {
#ifndef NDEBUG
    _verify_idx(idx);
#endif
    return _ptr[idx];
  };

  constexpr auto& at(size_t idx) { return _verify_idx(idx), _ptr[idx]; }
  constexpr auto& at(size_t idx) const { return _verify_idx(idx), _ptr[idx]; }

 private:
  constexpr void _verify_idx(size_t idx) const {
    if (idx >= _size) { throw std::out_of_range{"bad index"}; }
  }

 private:
  Ty_*   _ptr;
  size_t _size;
};

template <typename Range_>
constexpr auto make_view(Range_&& array) {
  return array_view{array.data(), array.size()};
}
}  // namespace KANGSW_ARRAY_VIEW_NAMESPACE
