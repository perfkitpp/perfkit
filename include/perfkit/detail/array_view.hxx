//
// Created by Seungwoo on 2021-08-26.
//
#pragma once
#include <cstddef>
#include <type_traits>

namespace KANGSW_ARRAY_VIEW_NAMESPACE {

template <typename Ty_>
class array_view {
 public:
  using value_type    = std::remove_const_t<Ty_>;
  using pointer       = value_type*;
  using const_pointer = value_type const*;
  using reference     = value_type&;

 public:
  constexpr array_view() noexcept = default;
  constexpr array_view(Ty_* p, size_t n) noexcept
      : _ptr(p), _n(n) {}

  template <typename Range_>
  constexpr array_view(Range_&& p) noexcept
      : array_view(p.data(), p.size()) {}

  constexpr auto size() const noexcept { return _n; }
  constexpr auto data() const noexcept { return _ptr; }
  constexpr auto data() noexcept { return _ptr; }

  constexpr auto begin() noexcept { return _ptr; }
  constexpr auto begin() const noexcept { return _ptr; }
  constexpr auto end() noexcept { return _ptr + _n; }
  constexpr auto end() const noexcept { return _ptr + _n; }

  constexpr auto& operator[](size_t idx) noexcept {
#ifndef NDEBUG
    _verify_idx(idx);
#endif
    return _ptr[idx];
  };

  constexpr auto& operator[](size_t idx) const noexcept {
#ifndef NDEBUG
    _verify_idx(idx);
#endif
    return _ptr[idx];
  };

  constexpr auto& at(size_t idx) noexcept { return _verify_idx(idx), _ptr[idx]; }
  constexpr auto& at(size_t idx) const noexcept { return _verify_idx(idx), _ptr[idx]; }

 private:
  constexpr void _verify_idx(size_t idx) {
    if (idx >= _n) { throw std::out_of_range{""}; }
  }

 private:
  Ty_*   _ptr;
  size_t _n;
};

}  // namespace KANGSW_ARRAY_VIEW_NAMESPACE
