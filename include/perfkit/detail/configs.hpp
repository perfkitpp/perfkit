//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <any>
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "perfkit/common/array_view.hxx"

namespace perfkit {
using json = nlohmann::json;

namespace detail {
class config_base;
}

class config_registry;
using config_ptr  = std::shared_ptr<detail::config_base>;
using config_wptr = std::weak_ptr<detail::config_base>;
using std::shared_ptr;
using std::weak_ptr;

namespace detail {

/**
 * basic config class
 *
 * TODO: Attribute retrieval for config class
 */
class config_base {
 public:
  using deserializer = std::function<bool(nlohmann::json const&, void*)>;
  using serializer   = std::function<void(nlohmann::json&, void const*)>;

 public:
  config_base(class config_registry* owner,
              void* raw,
              std::string full_key,
              std::string description,
              deserializer fn_deserial,
              serializer fn_serial,
              nlohmann::json&& attribute);

  /**
   * @warning this function is not re-entrant!
   * @return
   */
  nlohmann::json serialize();
  void serialize(nlohmann::json&);
  void serialize(std::function<void(nlohmann::json const&)> const&);

  nlohmann::json const& attribute() const noexcept { return _attribute; }
  nlohmann::json const& default_value() const { return _attribute["default"]; }

  bool consume_dirty() { return _dirty && !(_dirty = false); }

  auto const& full_key() const { return _full_key; }
  auto const& display_key() const { return _display_key; }
  auto const& description() const { return _description; }
  auto tokenized_display_key() const { return make_view(_categories); }
  void request_modify(nlohmann::json js);

  size_t num_modified() const { return _fence_modified.load(std::memory_order_relaxed); };
  bool is_transient() const noexcept { return attribute().contains("transient"); }

  /**
   * Check if latest marshalling result was invalid
   * @return
   */
  bool latest_marshal_failed() const {
    return _latest_marshal_failed.load(std::memory_order_relaxed);
  }

 private:
  bool _try_deserialize(nlohmann::json const& value);
  static void _split_categories(std::string_view view, std::vector<std::string_view>& out);

 private:
  friend class perfkit::config_registry;
  perfkit::config_registry* _owner;

  std::string _full_key;
  std::string _display_key;
  std::string _description;
  void* _raw;
  bool _dirty                             = true;  // default true to trigger initialization
  std::atomic_bool _latest_marshal_failed = false;

  std::atomic_size_t _fence_modified = 0;
  size_t _fence_serialized           = ~size_t{};
  nlohmann::json _cached_serialized;
  nlohmann::json _attribute;

  std::vector<std::string_view> _categories;

  deserializer _deserialize;
  serializer _serialize;
};
}  // namespace detail

/**
 *
 */
namespace configs {
// clang-format off
struct duplicated_flag_binding : std::logic_error { using std::logic_error::logic_error; };
struct invalid_flag_name : std::logic_error { using std::logic_error::logic_error; };
struct parse_error : std::runtime_error { using std::runtime_error::runtime_error; };
struct parse_help : parse_error { using parse_error::parse_error; };
// clang-format on

using flag_binding_table = std::map<std::string, config_ptr, std::less<>>;
flag_binding_table& _flags() noexcept;

void parse_args(int* argc, char*** argv, bool consume, bool ignore_undefined = false);
void parse_args(std::vector<std::string_view>* args, bool consume, bool ignore_undefined = false);

void import_from(const json& data);
json export_all();

bool import_from(std::string_view path);
bool export_to(std::string_view path);
}  // namespace configs

/**
 *
 */
class config_registry : std::enable_shared_from_this<config_registry> {
 public:
  using json_table        = std::map<std::string_view, nlohmann::json>;
  using config_table      = std::map<std::string_view, std::shared_ptr<detail::config_base>>;
  using string_view_table = std::map<std::string_view, std::string_view>;
  using container         = std::map<std::string, weak_ptr<config_registry>, std::less<>>;

 private:
  explicit config_registry(std::string name);

 public:
  ~config_registry() noexcept;

 public:
  bool update();
  void export_to(nlohmann::json*);
  void import_from(nlohmann::json);

  auto const& name() const { return _name; }

 public:
  bool bk_queue_update_value(std::string_view full_key, json value);
  std::string_view bk_find_key(std::string_view display_key);
  auto const& bk_all() const noexcept { return _entities; }
  uint64_t bk_schema_hash() const noexcept { return _schema_hash; }

 public:
  static auto bk_enumerate_registries() noexcept -> std::vector<std::shared_ptr<config_registry>>;
  static auto bk_find_reg(std::string_view name) noexcept -> shared_ptr<config_registry>;

  // TODO: deprecate this!
  static shared_ptr<config_registry> create(std::string name);

 public:  // for internal use only.
  auto _access_lock() { return std::unique_lock{_update_lock}; }

 public:
  void _put(std::shared_ptr<detail::config_base> o);
  bool _initially_updated() const noexcept { return _initial_update_done.load(); }

 private:
  std::string _name;
  config_table _entities;
  string_view_table _disp_keymap;
  std::set<detail::config_base*> _pending_updates;
  std::mutex _update_lock;

  // this value is used for identifying config registry's schema type, as config registry's
  //  layout never changes after updated once.
  uint64_t _schema_hash;

  // since configurations can be loaded before registry instance loaded, this flag makes
  //  the first update of registry to apply loaded configurations.
  std::atomic_bool _initial_update_done{false};
};

namespace _attr_flag {
enum ty : uint64_t {
  has_min      = 0x01 << 0,
  has_max      = 0x01 << 1,
  has_validate = 0x01 << 2,
  has_one_of   = 0x01 << 3,
};
}

enum class _config_flag_type {
  persistent,
  transient
};

template <typename Ty_>
class config;

template <typename Ty_>
struct _config_attrib_data {
  std::string description;
  std::function<bool(Ty_&)> validate;
  std::optional<Ty_> min;
  std::optional<Ty_> max;
  std::optional<std::set<Ty_>> one_of;

  std::optional<std::pair<_config_flag_type, std::vector<std::string>>>
          flag_binding;
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

  template <typename... Str_>
  auto& make_flag(bool save_to_config, Str_&&... args) {
    std::vector<std::string> flags;
    (flags.emplace_back(std::forward<Str_>(args)), ...);

    _data.flag_binding.emplace(
            std::make_pair(not save_to_config
                                   ? _config_flag_type::transient
                                   : _config_flag_type::persistent,
                           std::move(flags)));
    return *this;
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
    config_registry* dispatcher = {};
    std::string full_key        = {};
    Ty_ default_value           = {};
  };

  std::shared_ptr<_init_info> _pinfo;
};

template <typename Ty_>
class config {
 public:
 public:
  template <typename Attr_ = _config_factory<Ty_>>
  config(
          config_registry& repo,
          std::string full_key,
          Ty_&& default_value,
          _config_attrib_data<Ty_> attribute) noexcept
          : _owner(&repo), _value(std::forward<Ty_>(default_value)) {
    auto description = std::move(attribute.description);

    // define serializer
    detail::config_base::serializer fn_d = [this](nlohmann::json& out, const void* in) {
      out = *(Ty_*)in;
    };

    // set reference attribute
    nlohmann::json js_attrib;
    js_attrib["default"] = _value;

    if constexpr (Attr_::flag & _attr_flag::has_min) {
      js_attrib["min"] = *attribute.min;
    }
    if constexpr (Attr_::flag & _attr_flag::has_max) {
      js_attrib["max"] = *attribute.max;
    }
    if constexpr (Attr_::flag & _attr_flag::has_one_of) {
      js_attrib["oneof"] = *attribute.oneof;
    }
    if constexpr (Attr_::flag & _attr_flag::has_validate) {
      js_attrib["has_custom_validator"] = true;
    } else {
      js_attrib["has_custom_validator"] = false;
    }

    if (attribute.flag_binding) {
      std::vector<std::string>& binding = attribute.flag_binding->second;
      js_attrib["is_flag"]              = true;
      if (attribute.flag_binding->first == _config_flag_type::transient) {
        js_attrib["transient"] = true;
      }
      if (not binding.empty()) {
        js_attrib["flag_binding"] = std::move(binding);
      }
    }

    // setup marshaller / de-marshaller with given rule of attribute
    detail::config_base::deserializer fn_m = [attrib = std::move(attribute)]  //
            (const nlohmann::json& in, void* out) {
              try {
                Ty_ parsed;

                _config_attrib_data<Ty_> const& attr = attrib;
                nlohmann::from_json(in, parsed);

                if constexpr (Attr_::flag & _attr_flag::has_min) {
                  parsed = std::max<Ty_>(*attr.min, parsed);
                }
                if constexpr (Attr_::flag & _attr_flag::has_max) {
                  parsed = std::min<Ty_>(*attr.max, parsed);
                }
                if constexpr (Attr_::flag & _attr_flag::has_one_of) {
                  auto& oneof = attr->oneof;
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

    // instantiate config instance
    _opt = std::make_shared<detail::config_base>(
            _owner,
            &_value,
            std::move(full_key),
            std::move(description),
            std::move(fn_m),
            std::move(fn_d),
            std::move(js_attrib));

    // put instance to global queue
    repo._put(_opt);
  }

  config(const config&) noexcept = delete;
  config(config&&) noexcept      = default;

  /**
   * @warning
   *    Reading Ty_ and invocation of update() MUST occur on same thread!
   *
   * @return
   */
  Ty_ const& get() const noexcept { return _value; }
  Ty_ const& value() const noexcept { return _value; }

  /**
   * Provides thread-safe access for configuration.
   *
   * @return
   */
  Ty_ copy() const noexcept { return _owner->_access_lock(), Ty_{_value}; }

  Ty_ const& operator*() const noexcept { return get(); }
  Ty_ const* operator->() const noexcept { return &get(); }
  explicit operator const Ty_&() const noexcept { return get(); }

  bool check_dirty_and_consume() const { return _opt->consume_dirty(); }
  bool check_update() const { return _opt->consume_dirty(); }
  void async_modify(Ty_ v) { _owner->bk_queue_update_value(_opt->full_key(), std::move(v)); }

  auto& base() const { return *_opt; }

 private:
  config_ptr _opt;

  config_registry* _owner;
  Ty_ _value;
};

template <typename Ty_>
using _cvt_ty = std::conditional_t<
        std::is_convertible_v<Ty_, std::string>,
        std::string,
        std::remove_reference_t<Ty_>>;

template <typename Ty_>
auto configure(config_registry& dispatcher,
               std::string&& full_key,
               Ty_&& default_value) noexcept {
  _config_factory<_cvt_ty<Ty_>> attribute;
  attribute._pinfo                = std::make_shared<typename _config_factory<_cvt_ty<Ty_>>::_init_info>();
  attribute._pinfo->dispatcher    = &dispatcher;
  attribute._pinfo->default_value = std::move(default_value);
  attribute._pinfo->full_key      = std::move(full_key);

  return attribute;
}

}  // namespace perfkit
