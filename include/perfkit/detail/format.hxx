#pragma once
#include <spdlog/fmt/fmt.h>

namespace perfkit {
namespace util {

class format_context {
 public:
  template <typename... Args_>
  struct _proxy {
    using tuple_type = std::tuple<Args_...>;

    format_context* _base;
    tuple_type _tup;

   private:
    template <typename Ty_, size_t... Idx_>
    auto _concat_tuple(Ty_&& arg, std::index_sequence<Idx_...>) noexcept {
      return std::forward_as_tuple(std::get<Idx_>(std::move(_tup))..., std::forward<Ty_>(arg));
    }

   public:
    template <typename Ty_>
    auto operator%(Ty_&& arg) noexcept {
      using indices_t
              = std::make_index_sequence<std::tuple_size_v<tuple_type>>;

      return _proxy<Args_..., decltype(arg)>{
              _base, _concat_tuple(std::forward<Ty_>(arg), indices_t{})};
    }

    auto& operator>>(std::string& arg) noexcept {
      std::apply(
              [&](auto&&... args) {
                fmt::format_to(std::back_inserter(arg), _base->_fmt,
                               std::forward<decltype(args)>(args)...);
              },
              std::move(_tup));
      return arg;
    }

    auto&& operator>>(std::string&& arg) noexcept {
      return std::move((*this) >> (reinterpret_cast<std::string&>(arg)));
    }

    auto& operator>(std::string& arg) noexcept {
      arg.clear();
      std::apply(
              [&](auto&&... args) {
                fmt::format_to(std::back_inserter(arg), _base->_fmt,
                               std::forward<decltype(args)>(args)...);
              },
              std::move(_tup));
      return arg;
    }

    auto&& operator>(std::string&& arg) noexcept {
      return std::move((*this) > (reinterpret_cast<std::string&>(arg)));
    }

    auto operator/(size_t init_cap) noexcept {
      std::string str;
      str.reserve(init_cap);
      return (*this) > str;
    }

    template <typename... Args2_>
    std::string operator()(Args2_&&... args) {
      return fmt::format(_base->_fmt, std::forward<Args2_>(args)...);
    }

    operator std::string() {
      return std::apply(*this, std::move(_tup));
    }

    std::string string() const { return (std::string)(*this); }
  };

 public:
  // format with arbitrary types
  template <typename Ty_>
  auto operator%(Ty_&& arg) noexcept {
    return _proxy<decltype(arg)>{this, std::forward_as_tuple(std::forward<Ty_>(arg))};
  }

  template <typename... Args_>
  auto operator()(Args_&&... args) noexcept {
    return _proxy<decltype(args)...>{this, std::forward_as_tuple(std::forward<Args_>(args)...)};
  }

 public:
  char const* const _fmt = {};
};

template <typename Str_, typename... Args_>
auto&& operator<(Str_&& str, format_context::_proxy<Args_...>&& prx) {
  return prx.operator>(std::forward<Str_>(str));
}

template <typename Str_, typename... Args_>
auto&& operator<<(Str_&& str, format_context::_proxy<Args_...>&& prx) {
  return prx.operator>>(std::forward<Str_>(str));
}

}  // namespace util

inline namespace literals {
inline util::format_context operator""_fmt(char const* ch, size_t) {
  return {ch};
}
}  // namespace literals
}  // namespace perfkit