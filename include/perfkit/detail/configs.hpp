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

#include "array_view.hxx"

namespace perfkit {
class config_registry;

namespace detail {

/**
 * basic config class
 */
class config_base {
 public:
  using deserializer = std::function<bool(nlohmann::json const&, void*)>;
  using serializer   = std::function<void(nlohmann::json&, void const*)>;

 public:
  config_base(class config_registry* owner,
              void*                  raw,
              std::string            full_key,
              std::string            description,
              deserializer           fn_deserial,
              serializer             fn_serial);

  /**
   * @warning this function is not re-entrant!
   * @return
   */
  nlohmann::json serialize();
  void           serialize(nlohmann::json&);
  void           serialize(std::function<void(nlohmann::json const&)> const&);

  bool consume_dirty() { return _dirty && !(_dirty = false); }

  std::string_view full_key() const { return _full_key; }
  std::string_view display_key() const { return _display_key; }
  std::string_view description() const { return _description; }
  auto             tokenized_display_key() const { return make_view(_categories); }
  void             request_modify(nlohmann::json const& js);

  size_t num_modified() const { return _fence_modification.load(std::memory_order_relaxed); };

  /**
   * Check if latest marshalling result was invalid
   * @return
   */
  bool latest_marshal_failed() const {
    return _latest_marshal_failed.load(std::memory_order_relaxed);
  }

 private:
  bool        _try_deserialize(nlohmann::json const& value);
  static void _split_categories(std::string_view view, std::vector<std::string_view>& out);

 private:
  friend class perfkit::config_registry;
  perfkit::config_registry* _owner;

  std::string      _full_key;
  std::string      _display_key;
  std::string      _description;
  void*            _raw;
  bool             _dirty                 = true;  // default true to trigger initialization
  std::atomic_bool _latest_marshal_failed = false;

  std::atomic_size_t _fence_modification = 0;
  size_t             _fence_serialized   = ~size_t{};
  nlohmann::json     _cached_serialized;

  std::vector<std::string_view> _categories;

  deserializer _deserialize;
  serializer   _serialize;
};
}  // namespace detail

/**
 *
 */
class config_registry {
 public:
  using json_table   = std::map<std::string, nlohmann::json>;
  using config_table = std::map<std::string_view, std::shared_ptr<detail::config_base>>;
  using container    = std::vector<std::unique_ptr<config_registry>>;

 public:
  bool apply_update_and_check_if_dirty();

  void queue_update_value(std::string full_key, nlohmann::json const& value);

  static bool request_update_value(std::string full_key, nlohmann::json const& value);

  static std::string_view find_key(std::string_view display_key);

  static bool check_dirty_and_consume_global();

  static config_registry& create() noexcept;
  static config_table&    all() noexcept;

 public:  // for internal use only.
  auto _access_lock() { return std::unique_lock{_update_lock}; }

 public:
  void _put(std::shared_ptr<detail::config_base> o);

 private:
  config_table _opts;
  json_table   _pending_updates;
  std::mutex   _update_lock;

  static inline std::atomic_bool _global_dirty;
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
class config;

template <typename Ty_>
struct _config_attrib_data {
  std::string                  description;
  std::function<bool(Ty_&)>    validate;
  std::optional<Ty_>           min;
  std::optional<Ty_>           max;
  std::optional<std::set<Ty_>> one_of;
};

template <typename Ty_, uint64_t Flags_ = 0>
class _config_factory {
 public:
  enum { flag = Flags_ };

 private:
  template <uint64_t Flag_>
  auto& _added() { return *reinterpret_cast<_config_factory<Ty_, Flags_ | Flag_>*>(this); }

 public:
  auto& description(std::string&& s) { return _data.description = std::move(s), *this; }
  auto& min(Ty_ v) { return _data.min = v, _added<_attr_flag::has_min>(); }
  auto& max(Ty_ v) { return _data.max = v, _added<_attr_flag::has_max>(); }
  auto& one_of(std::initializer_list<Ty_> v) {
    _data.one_of.emplace();
    _data.one_of->insert(v.begin(), v.end());
    return _added<_attr_flag::has_one_of>();
  }
  auto& validate(std::function<bool(nlohmann::json&)>&& v) {
    _data.validate = std::move(v);
    return _added<_attr_flag::has_validate>();
  }

  auto confirm() noexcept {
    return config<Ty_>{
        *_pinfo->dispatcher,
        std::move(_pinfo->full_key),
        std::forward<Ty_>(_pinfo->default_value),
        std::move(_data)};
  }

 private:
  template <typename>
  friend class config;

  _config_attrib_data<Ty_> _data;

 public:
  struct _init_info {
    config_registry* dispatcher    = {};
    std::string      full_key      = {};
    Ty_              default_value = {};
  };

  std::shared_ptr<_init_info> _pinfo;
};

using config_ptr = std::shared_ptr<detail::config_base>;

template <typename Ty_>
class config {
 public:
 public:
  template <typename Attr_ = _config_factory<Ty_>>
  config(
      config_registry&         dispatcher,
      std::string              full_key,
      Ty_&&                    default_value,
      _config_attrib_data<Ty_> attribute) noexcept
      : _owner(&dispatcher), _value(std::forward<Ty_>(default_value)) {
    auto description = std::move(attribute.description);

    // setup marshaller / de-marshaller with given rule of attribute
    detail::config_base::deserializer fn_m = [attrib = std::move(attribute)]  //
        (const nlohmann::json& in, void* out) {
          // TODO: Apply attributes
          try {
            Ty_ parsed;

            _config_attrib_data<Ty_> const& attr = attrib;
            nlohmann::from_json(in, parsed);

            if constexpr (Attr_::flag & _attr_flag::has_min) {
              parsed = std::min<Ty_>(*attr.min, parsed);
            }
            if constexpr (Attr_::flag & _attr_flag::has_max) {
              parsed = std::max<Ty_>(*attr.min, parsed);
            }
            if constexpr (Attr_::flag & _attr_flag::has_one_of) {
              auto oneof = attr->oneof;
              if (oneof.find(parsed) == oneof.end()) { return false; }
            }
            if constexpr (Attr_::flag & _attr_flag::has_validate) {
              if (!attr.validate(parsed)) { return false; }
            }

            *(Ty_*)out = parsed;
            return true;
          } catch (std::exception&) {
            return false;
          }
        };

    detail::config_base::serializer fn_d = [this](nlohmann::json& out, const void* in) {
      out = *(Ty_*)in;
    };

    // instantiate config instance
    _opt = std::make_shared<detail::config_base>(
        _owner,
        &_value,
        std::move(full_key),
        std::move(description),
        std::move(fn_m),
        std::move(fn_d));
    // put instance to global queue
    dispatcher._put(_opt);
  }

  config(const config&) noexcept = delete;
  config(config&&) noexcept      = default;

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
  void async_modify(Ty_ v) { _owner->queue_update_value(std::string{_opt->full_key()}, std::move(v)); }

  auto& base() const { return *_opt; }

 private:
  config_ptr _opt;

  config_registry* _owner;
  Ty_              _value;
};

template <typename Ty_>
using _cvt_ty = std::conditional_t<
    std::is_convertible_v<Ty_, std::string>,
    std::string,
    Ty_>;

template <typename Ty_>
auto configure(config_registry& dispatcher,
               std::string&&    full_key,
               Ty_&&            default_value) noexcept {
  _config_factory<_cvt_ty<Ty_>> attribute;
  attribute._pinfo                = std::make_shared<typename _config_factory<_cvt_ty<Ty_>>::_init_info>();
  attribute._pinfo->dispatcher    = &dispatcher;
  attribute._pinfo->default_value = std::move(default_value);
  attribute._pinfo->full_key      = std::move(full_key);

  return attribute;
}

}  // namespace perfkit