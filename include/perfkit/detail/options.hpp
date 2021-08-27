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
  using marshal_function   = std::function<bool(nlohmann::json const&, void*)>;
  using unmarshal_function = std::function<void(nlohmann::json&, void const*)>;

 public:
  option_base(void*              raw,
              std::string        full_key,
              std::string        description,
              marshal_function   fn_m,
              unmarshal_function fn_d)
      : _full_key(std::move(full_key)),
        _description(std::move(description)),
        _raw(raw),
        _marshal(std::move(fn_m)),
        _unmarshal(std::move(fn_d)) {}

  void unmarshal(nlohmann::json& out) {
    _unmarshal(out, _raw);
  }

  bool consume_dirty() { return _dirty && !(_dirty = false); }

  std::string_view full_key() const { return _full_key; }

  std::string_view description() const { return description(); }

  /**
   * Check if latest marshalling result was invalid
   * @return
   */
  bool latest_marshal_failed() const {
    return _latest_marshal_failed.load(std::memory_order_relaxed);
  }

 public:
  bool _try_marshal(nlohmann::json const& value) {
    return !(_latest_marshal_failed.store(_marshal(value, _raw) && (_dirty = true),
                                          std::memory_order_relaxed),
             _latest_marshal_failed.load(std::memory_order_relaxed));
  }

 private:
  std::string      _full_key;
  std::string      _description;
  void*            _raw;
  bool             _dirty                 = false;
  std::atomic_bool _latest_marshal_failed = false;

  marshal_function   _marshal;
  unmarshal_function _unmarshal;
};
}  // namespace detail

/**
 *
 */
class option_dispatcher {
 public:
  using json_table   = std::map<std::string, nlohmann::json>;
  using option_table = std::map<std::string_view, std::shared_ptr<detail::option_base>>;
  using container    = std::vector<std::unique_ptr<option_dispatcher>>;

 public:
  bool apply_update_and_check_if_dirty();

  void queue_update_value(std::string full_key, nlohmann::json const& value) {
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
  static option_table&      _all() noexcept;

 public:
  void _put(std::shared_ptr<detail::option_base> o);

 private:
  option_table _opts;
  json_table   _pending_updates;
  std::mutex   _update_lock;
};

namespace _attr_flag {
enum ty : uint64_t {
  has_min      = 0x01,
  has_max      = 0x02,
  has_validate = 0x04,
  has_one_of   = 0x08,
};

}

template <typename Ty_>
class option;

template <typename Ty_>
struct _option_attribute_data {
  std::string                          description;
  std::function<bool(nlohmann::json&)> validate;
  std::optional<Ty_>                   min;
  std::optional<Ty_>                   max;
  std::optional<std::set<Ty_>>         one_of;
};

template <typename Ty_, uint64_t Flags_ = 0>
class attribute {
 public:
  enum { flag = Flags_ };

 private:
  template <uint64_t Flag_>
  auto& _added() { return *reinterpret_cast<attribute<Ty_, Flags_ | Flag_>*>(this); }

 public:
  auto& description(std::string s) { return _data.description = std::move(s), *this; }
  auto& min(Ty_ v) { return _data.min = v, _added<_attr_flag::has_min>(); }
  auto& max(Ty_ v) { return _data.max = v, _added<_attr_flag::has_max>(); }
  auto& one_of(std::initializer_list<Ty_> v) {
    _data.one_of->insert(v.begin(), v.end());
    return _added<_attr_flag::has_one_of>();
  }
  auto& validate(std::function<bool(nlohmann::json&)> v) {
    _data.validate = std::move(v);
    return _added<_attr_flag::has_validate>();
  }

 private:
  template <typename>
  friend class option;

  _option_attribute_data<Ty_> _data;
};

template <typename Ty_>
class option {
 public:
 public:
  template <typename Attr_ = attribute<Ty_>>
  option(
      option_dispatcher& dispatcher,
      std::string        full_key,
      Ty_                default_value,
      Attr_&&            attr = attribute<Ty_>{}) noexcept
      : _owner(&dispatcher),
        _value(default_value) {
    auto description = std::move(attr._data.description);

    // setup marshaller / de-marshaller with given rule of attribute
    detail::option_base::marshal_function fn_m = [attrib = std::move(attr)]  //
        (const nlohmann::json& in, void* out) {
          // TODO: Apply attributes
          try {
            Ty_ parsed;
            nlohmann::from_json(in, parsed);

            if constexpr (Attr_::flag & _attr_flag::has_min) {
              // TODO
            }
            if constexpr (Attr_::flag & _attr_flag::has_max) {
              // TODO
            }
            if constexpr (Attr_::flag & _attr_flag::has_one_of) {
              // TODO
            }
            if constexpr (Attr_::flag & _attr_flag::has_validate) {
              // TODO
            }

            *(Ty_*)out = parsed;
            return true;
          } catch (std::exception&) {
            return false;
          }
        };

    detail::option_base::unmarshal_function fn_d = [this](nlohmann::json& out, const void* in) {
      _owner->access_lock(), out = *(Ty_*)in;
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

  /**
   * @warning
   *    Reading Ty_ and invocation of apply_update_and_check_if_dirty() MUST occur on same thread!
   *
   * @return
   */
  Ty_ const& get() const noexcept { return _value; }
  Ty_ const& operator*() const noexcept { return get(); }
  Ty_ const* operator->() const noexcept { return &get(); }
  explicit   operator const Ty_&() const noexcept { return get(); }

  bool check_dirty_and_consume() const { return _opt->consume_dirty(); }
  void queue_change_value(Ty_ v) { _owner->queue_update_value(_opt->full_key(), std::move(v)); }

 private:
  std::shared_ptr<detail::option_base> _opt;

  option_dispatcher* _owner;
  Ty_                _value;
};

template <typename Ty_>
using _cvt_ty = std::conditional_t<std::is_convertible_v<Ty_, std::string>,
                                   std::string,
                                   Ty_>;

template <typename Ty_, typename Attr_ = attribute<_cvt_ty<Ty_>>>
auto declare_option(option_dispatcher& dispatcher,
                    std::string        full_key,
                    Ty_&&              default_value,
                    Attr_&&            attr = attribute<_cvt_ty<Ty_>>{}) {
  return option<_cvt_ty<Ty_>>{
      dispatcher,
      std::move(full_key),
      std::forward<Ty_>(default_value),
      std::move(attr)};
}

}  // namespace perfkit
