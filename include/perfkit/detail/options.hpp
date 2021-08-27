//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <any>
#include <atomic>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace perfkit {
namespace detail {
/**
 * basic option class
 */
class option_base {
 public:
  using marshal_function = std::function<bool(nlohmann::json const&, void*)>;
  using unmarshal_function = std::function<void(nlohmann::json&, void const*)>;

 public:
  option_base(void* raw,
              std::string full_key,
              std::string description,
              marshal_function fn_m,
              unmarshal_function fn_d)
      : _full_key(std::move(full_key)),
        _description(std::move(description)),
        _raw(raw),
        _marshal(std::move(fn_m)),
        _unmarshal(std::move(fn_d)) {}

  bool try_marshal(nlohmann::json const& value) {
    return !(_latest_marshal_failed = (_marshal(value, _raw) && (_dirty = true)));
  }

  void unmarshal(nlohmann::json& out) {
    _unmarshal(out, _raw);
  }

  bool consume_dirty() { return _dirty && !(_dirty = false); }

  std::string_view full_key() const { return _full_key; }

  /**
   * Check if latest marshalling result was invalid
   * @return
   */
  bool latest_marshal_failed() const {
    return _latest_marshal_failed.load(std::memory_order_relaxed);
  }

 private:
  std::string _full_key;
  std::string _description;
  void* _raw;
  bool _dirty = false;
  std::atomic_bool _latest_marshal_failed = false;

  marshal_function _marshal;
  unmarshal_function _unmarshal;
};
}  // namespace detail

/**
 *
 */
class option_dispatcher {
 public:
  using json_table = std::map<std::string, nlohmann::json>;
  using option_table = std::map<std::string_view, std::shared_ptr<detail::option_base>>;
  using container = std::vector<std::unique_ptr<option_dispatcher>>;

 public:
  bool apply_update_and_check_if_dirty();

  void queue_update(std::string full_key, nlohmann::json const& value) {
    std::unique_lock _l{_update_lock};
    _pending_updates[std::move(full_key)] = value;
  }

  /**
   * To prevent race condition of external option value access, this makes lock that
   * will block the update of queued values until release.
   *
   * @return
   */
  auto access_lock() { return std::unique_lock{_update_lock}; }

 public:
  static option_dispatcher& _create() noexcept;
  static option_table& _all() noexcept;

 public:
  void _put(std::shared_ptr<detail::option_base> o);

 private:
  option_table _opts;
  json_table _pending_updates;
  std::mutex _update_lock;
};

template <typename Ty_>
class option {
 public:
  struct attribute {
    std::function<bool(nlohmann::json&)> validate;
    std::optional<Ty_> min;
    std::optional<Ty_> max;
    std::optional<std::set<Ty_>> one_of;
  };

 public:
  option(
      option_dispatcher& dispatcher,
      std::string full_key,
      Ty_ default_value,
      std::string description = {},
      attribute attr = {}) noexcept
      : _value(default_value) {
    // setup marshaller / de-marshaller with given rule of attribute
    detail::option_base::marshal_function fn_m = [](const nlohmann::json& in, void* out) {
      // TODO: Apply attributes
      try {
        nlohmann::from_json(in, *(Ty_*)out);
        return true;
      } catch (std::exception&) {
        return false;
      }
    };

    detail::option_base::unmarshal_function fn_d = [](nlohmann::json& out, const void* in) {
      out = *(Ty_*)in;
    };

    // instantiate option instance
    _opt = std::make_shared<detail::option_base>(
        &_value,
        full_key,
        std::move(description),
        std::move(fn_m),
        std::move(fn_d));

    // put instance to global queue
    dispatcher._put(_opt);
  }

  Ty_ const& get() const noexcept { return _value; }
  operator Ty_() const noexcept { return get(); }
  Ty_ const& operator*() const noexcept { return get(); }
  Ty_ const* operator->() const noexcept { return &get(); }

  bool check_dirty_and_consume() const { return _opt->consume_dirty(); }

 private:
  std::shared_ptr<detail::option_base> _opt;
  Ty_ _value;
};

}  // namespace perfkit
