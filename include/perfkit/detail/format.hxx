#pragma once
#include <spdlog/fmt/fmt.h>

namespace perfkit {
namespace util {

class format_context {
 public:
  template <typename... Args_>
  struct _proxy {
    using tuple_type = std::tuple<Args_...>;

    char const* _fmt;
    tuple_type _tup;

    template <typename... Args2_>
    std::string operator()(Args2_&&... args) const {
      return fmt::format(_fmt, std::forward<Args2_>(args)...);
    }

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
              _fmt, _concat_tuple(std::forward<Ty_>(arg), indices_t{})};
    }

    operator std::string() const {
      return str();
    }

    std::string str() const { return std::apply(*this, _tup); }
  };

 public:
  // format with arbitrary types
  template <typename Ty_>
  auto operator%(Ty_&& arg) const noexcept {
    return _proxy<decltype(arg)>{_fmt, std::forward_as_tuple(std::forward<Ty_>(arg))};
  }

  char const* const _fmt;
};

}  // namespace util

inline namespace literals {
util::format_context operator""_fmt(char const* ch, size_t) {
  return {ch};
}
}  // namespace literals
}  // namespace perfkit
