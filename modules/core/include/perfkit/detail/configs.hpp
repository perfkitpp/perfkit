// MIT License
//
// Copyright (c) 2021-2022. Seungwoo Kang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// project home: https://github.com/perfkitpp

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
#include <type_traits>
#include <utility>

#include <nlohmann/json.hpp>

#include "cpph/array_view.hxx"
#include "cpph/event.hxx"
#include "cpph/hasher.hxx"
#include "cpph/macros.hxx"
#include "cpph/spinlock.hxx"
#include "cpph/template_utils.hxx"
#include "perfkit/utils/config_watcher.hpp"

namespace perfkit {
using json = nlohmann::json;

namespace detail {
class config_base;
}

class config_registry;
using config_shared_ptr = std::shared_ptr<detail::config_base>;
using config_wptr = std::weak_ptr<detail::config_base>;
using std::shared_ptr;
using std::weak_ptr;

struct config_attribute_t {
    nlohmann::json default_value;
    std::string description;

    nlohmann::json one_of;
    nlohmann::json min;
    nlohmann::json max;

    //
    bool has_custom_validator = false;

    bool can_import = true;
    bool can_export = true;

    bool is_flag = false;
    bool hidden = false;

    //
    std::vector<std::string> flag_bindings;
};

namespace detail {
/**
 * basic config class
 *
 * TODO: Attribute retrieval for config class
 */
class config_base : public std::enable_shared_from_this<config_base>
{
   public:
    using deserializer = std::function<bool(nlohmann::json const&, void*)>;
    using serializer = std::function<void(nlohmann::json&, void const*)>;

   private:
    friend class perfkit::config_registry;
    perfkit::config_registry* _owner;

    std::string _full_key;
    std::string _display_key;
    void* _raw;
    std::atomic_bool _dirty = true;  // default true to trigger initialization
    std::atomic_bool _latest_marshal_failed = false;

    std::atomic_size_t _fence_modified = 0;
    std::atomic_size_t _fence_serialized = ~size_t{};
    config_attribute_t _attribute;
    nlohmann::json _cached_serialized;

    std::vector<std::string_view> _categories;

    deserializer _deserialize;
    serializer _serialize;

   public:
    config_base(class config_registry* owner,
                void* raw,
                std::string full_key,
                deserializer fn_deserial,
                serializer fn_serial,
                config_attribute_t&& attribute);

    /**
     * @warning this function is not re-entrant!
     * @return
     */
    nlohmann::json serialize();
    void serialize(nlohmann::json&);
    void serialize(std::function<void(nlohmann::json const&)> const&);

    auto const& attribute() const noexcept { return _attribute; }
    nlohmann::json const& default_value() const { return _attribute.default_value; }

    bool consume_dirty() { return _dirty.exchange(false); }

    auto const& full_key() const { return _full_key; }
    auto const& display_key() const { return _display_key; }
    auto const& description() const { return _attribute.description; }
    auto tokenized_display_key() const { return make_view(_categories); }
    void request_modify(nlohmann::json js);

    size_t num_modified() const { return _fence_modified; };
    size_t num_serialized() const { return _fence_serialized; }

    bool can_export() const noexcept { return _attribute.can_export; }
    bool can_import() const noexcept { return _attribute.can_import; }
    bool is_hidden() const noexcept { return _attribute.hidden; }

    /**
     * Check if latest marshalling result was invalid
     * @return
     */
    bool latest_marshal_failed() const
    {
        return _latest_marshal_failed.load(std::memory_order_relaxed);
    }

   private:
    bool _try_deserialize(nlohmann::json const& value);
    static void _split_categories(std::string_view view, std::vector<std::string_view>& out);
};
}  // namespace detail

/**
 *
 */
namespace configs {
CPPH_UNIQUE_KEY_TYPE(schema_hash_t);

// clang-format off
struct duplicated_flag_binding : std::logic_error { using std::logic_error::logic_error; };
struct invalid_flag_name : std::logic_error { using std::logic_error::logic_error; };
struct parse_error : std::runtime_error { using std::runtime_error::runtime_error; };
struct parse_help : parse_error { using parse_error::parse_error; };
struct schema_mismatch : parse_error { using parse_error::parse_error; };
// clang-format on

using flag_binding_table = std::map<std::string, config_shared_ptr, std::less<>>;
flag_binding_table& _flags() noexcept;

void parse_args(int* argc, char*** argv, bool consume, bool ignore_undefined = false);
void parse_args(std::vector<std::string_view>* args, bool consume, bool ignore_undefined = false);

bool import_from(json const& data);
json export_all();

bool import_file(std::string_view path);
bool export_to(std::string_view path);

/** wait until any configuration update is applied. */
perfkit::event<perfkit::config_registry*>&
on_new_config_registry();

}  // namespace configs

/**
 *
 */
class config_registry : public std::enable_shared_from_this<config_registry>
{
   public:
    using json_table = std::map<std::string_view, nlohmann::json>;
    using config_table = std::map<std::string_view, std::shared_ptr<detail::config_base>>;
    using string_view_table = std::map<std::string_view, std::string_view>;
    using container = std::map<std::string, weak_ptr<config_registry>, std::less<>>;

   private:
    enum class update_state {
        none,
        busy,
        ready
    };

   private:
    static inline std::atomic_size_t IDGEN = 0;

    size_t const _id = ++IDGEN;
    std::string _name;
    config_table _entities;
    string_view_table _disp_keymap;
    std::vector<detail::config_base*> _pending_updates;
    perfkit::spinlock _update_lock;

    // this value is used for identifying config registry's schema type, as config registry's
    //  layout never changes after updated once.
    std::type_info const* _schema_class;
    configs::schema_hash_t _schema_hash{hasher::FNV_OFFSET_BASE};

    // since configurations can be loaded before registry instance loaded, this flag makes
    //  the first update of registry to apply loaded configurations.
    std::atomic<update_state> _initial_update_state{update_state::none};

   private:
    explicit config_registry(std::string name);

   public:
    ~config_registry() noexcept;

   public:
    size_t id() const noexcept { return _id; }

    bool update();
    void export_to(nlohmann::json*);
    void import_from(nlohmann::json);

    auto const& name() const { return _name; }

   public:
    bool bk_queue_update_value(std::string_view full_key, json value);
    std::string_view bk_find_key(std::string_view display_key);
    auto const& bk_all() const noexcept { return _entities; }
    auto bk_schema_class() const noexcept { return _schema_class; }
    auto bk_schema_hash() const noexcept { return _schema_hash; }

    using update_event_t = event<config_registry*, array_view<detail::config_base*>>;
    using destroy_event_t = event<config_registry*>;

    update_event_t on_update;
    destroy_event_t on_destroy;

   public:
    static auto create(std::string name, std::type_info const* schema = nullptr) -> shared_ptr<config_registry>;

   public:
    static auto bk_enumerate_registries(bool filter_complete = false) noexcept -> std::vector<std::shared_ptr<config_registry>>;
    static auto bk_find_reg(std::string_view name) noexcept -> shared_ptr<config_registry>;

   public:  // for internal use only.
    auto _access_lock() { return std::unique_lock{_update_lock}; }

   public:
    void _put(std::shared_ptr<detail::config_base> o);
    bool _initially_updated() const noexcept { return _initial_update_state == update_state::ready; }
};

namespace _attr_flag {
enum ty : uint64_t {
    has_min = 0x01 << 0,
    has_max = 0x01 << 1,
    has_validate = 0x01 << 2,
    has_one_of = 0x01 << 3,
    has_verify = 0x01 << 4,
};
}

enum class _config_io_type {
    persistent,
    transient,
    transient_readonly,
};

template <typename Ty_>
class config;

template <typename Ty_>
struct _config_attrib_data {
    std::string description;
    std::function<bool(Ty_&)> validate;
    std::function<bool(Ty_ const&)> verify;
    std::optional<Ty_> min;
    std::optional<Ty_> max;
    std::optional<std::set<Ty_>> one_of;
    std::string env_name;
    bool hidden = false;

    std::optional<std::vector<std::string>> flag_binding;
    _config_io_type transient_type = _config_io_type::persistent;
};

template <typename Ty_, uint64_t Flags_ = 0>
class _config_factory
{
   public:
    enum {
        flag = Flags_
    };

   private:
    template <uint64_t Flag_>
    auto& _added()
    {
        return *reinterpret_cast<_config_factory<Ty_, Flags_ | Flag_>*>(this);
    }

   public:
    auto& description(std::string&& s) { return _data.description = std::move(s), *this; }

    auto& min(Ty_ v)
    {
        static_assert(not(Flags_ & _attr_flag::has_one_of));
        return _data.min = std::move(v), _added<_attr_flag::has_min>();
    }

    auto& max(Ty_ v)
    {
        static_assert(not(Flags_ & _attr_flag::has_one_of));
        return _data.max = std::move(v), _added<_attr_flag::has_max>();
    }

    /** value should be one of given entities */
    auto& one_of(std::initializer_list<Ty_> v)
    {
        static_assert(not(Flags_ & (_attr_flag::has_max | _attr_flag::has_min)));
        _data.one_of.emplace();
        _data.one_of->insert(v.begin(), v.end());
        return _added<_attr_flag::has_one_of>();
    }

    template <typename Range_>
    auto& one_of(Range_ const& r)
    {
        static_assert(not(Flags_ & (_attr_flag::has_max | _attr_flag::has_min)));
        _data.one_of.emplace();
        _data.one_of->insert(std::begin(r), std::end(r));
        return _added<_attr_flag::has_one_of>();
    }

    /**
     * Hidden elements, usually won't appear on
     */
    auto& hide()
    {
        _data.hidden = true;
        return *this;
    }

    /**
     * validate function makes value assignable to destination.
     * if callback returns false, change won't be applied (same as verify())
     */
    template <typename Callable_>
    auto& validate(Callable_&& v)
    {
        if constexpr (std::is_invocable_r_v<bool, Callable_, Ty_&>) {
            _data.validate = std::move(v);
        } else {
            _data.validate =
                    [fn = std::forward<Callable_>(v)](Ty_& s) {
                        fn(s);
                        return true;
                    };
        }
        return _added<_attr_flag::has_validate>();
    }

    /** verify function will discard updated value if verification failed. */
    auto& verify(std::function<bool(Ty_ const&)>&& v)
    {
        _data.verify = std::move(v);
        return _added<_attr_flag::has_verify>();
    }

    /** expose entities as flags. configs MUST NOT BE field of any config template class! */
    template <typename... Str_>
    auto& flags(Str_&&... args)
    {
        std::vector<std::string> flags;
        (flags.emplace_back(std::forward<Str_>(args)), ...);

        _data.flag_binding.emplace(std::move(flags));
        return *this;
    }

    /** deprecated. should be broken into transient() and flags(). */
    template <typename... Str_>
    [[deprecated]] auto&
    make_flag(bool save_to_config, Str_&&... args)
    {
        if (not save_to_config)
            transient();
        return flags(std::forward<Str_>(args)...);
    }

    /** transient marked configs won't be saved or loaded from config files. */
    auto& transient()
    {
        _data.transient_type = _config_io_type::transient;
        return *this;
    }

    /** readonly marked configs won't export, but still can import from config files. */
    auto& readonly()
    {
        _data.transient_type = _config_io_type::transient_readonly;
        return *this;
    }

    /** initialize from environment variable */
    auto& env(std::string s)
    {
        _data.env_name = std::move(s);
        return *this;
    }

    auto confirm() noexcept
    {
        return config<Ty_>{
                std::integral_constant<uint64_t, Flags_>{},
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
        std::string full_key = {};
        Ty_ default_value = {};
    };

    std::shared_ptr<_init_info> _pinfo;
};

template <typename Ty_>
class config
{
   public:
   public:
    template <uint64_t Flags_>
    config(
            std::integral_constant<uint64_t, Flags_>,
            config_registry& repo,
            std::string full_key,
            Ty_&& default_value,
            _config_attrib_data<Ty_> attribute) noexcept
            : _owner(&repo), _value(std::forward<Ty_>(default_value))
    {
        std::string env = std::move(attribute.env_name);

        // define serializer
        detail::config_base::serializer fn_d = [this](nlohmann::json& out, void const* in) {
            out = *(Ty_ const*)in;
        };

        // set reference attribute
        config_attribute_t conf_attrib = {};
        conf_attrib.default_value = _value;
        conf_attrib.description = attribute.description;

        if constexpr (!!(Flags_ & _attr_flag::has_min))
            conf_attrib.min = *attribute.min;
        if constexpr (!!(Flags_ & _attr_flag::has_max))
            conf_attrib.max = *attribute.max;
        if constexpr (!!(Flags_ & _attr_flag::has_one_of))
            conf_attrib.one_of = *attribute.one_of;

        if constexpr (!!(Flags_ & _attr_flag::has_validate)) {
            conf_attrib.has_custom_validator = true;
        } else {
            conf_attrib.has_custom_validator = false;
        }

        if (attribute.hidden) {
            conf_attrib.hidden = true;
        }

        if (attribute.flag_binding) {
            std::vector<std::string>& binding = *attribute.flag_binding;
            conf_attrib.is_flag = true;
            if (not binding.empty()) {
                conf_attrib.flag_bindings = std::move(binding);
            }
        }

        conf_attrib.can_export = attribute.transient_type == _config_io_type::persistent;
        conf_attrib.can_import = attribute.transient_type != _config_io_type::transient;

        // setup marshaller / de-marshaller with given rule of attribute
        detail::config_base::deserializer fn_m = [attrib = std::move(attribute)]  //
                (nlohmann::json const& in, void* out) {
                    try {
                        Ty_ parsed;
                        bool okay = true;

                        _config_attrib_data<Ty_> const& attr = attrib;

                        if constexpr (std::is_same_v<nlohmann::json, Ty_>)
                            parsed = in;
                        else
                            nlohmann::from_json(in, parsed);

                        if constexpr (!!(Flags_ & _attr_flag::has_min)) {
                            if constexpr (has_binary_op_v<std::less<>, Ty_>)
                                parsed = std::max<Ty_>(*attr.min, parsed);
                        }
                        if constexpr (!!(Flags_ & _attr_flag::has_max)) {
                            if constexpr (has_binary_op_v<std::less<>, Ty_>)
                                parsed = std::min<Ty_>(*attr.max, parsed);
                        }
                        if constexpr (!!(Flags_ & _attr_flag::has_one_of)) {
                            if (attr.one_of->find(parsed) == attr.one_of->end())
                                return false;
                        }
                        if constexpr (!!(Flags_ & _attr_flag::has_verify)) {
                            if (not attr.verify(parsed))
                                return false;
                        }
                        if constexpr (!!(Flags_ & _attr_flag::has_validate)) {
                            okay |= attr.validate(parsed);  // value should be validated
                        }

                        *(Ty_*)out = std::move(parsed);
                        return okay;
                    } catch (std::exception&) {
                        return false;
                    }
                };

        // instantiate config instance
        _opt = std::make_shared<detail::config_base>(
                _owner,
                &_value,
                std::move(full_key),
                std::move(fn_m),
                std::move(fn_d),
                std::move(conf_attrib));

        // put instance to global queue
        repo._put(_opt);

        // queue environment value if available
        if (not env.empty())
            if (auto env_value = getenv(env.c_str())) {
                nlohmann::json parsed_json;
                if constexpr (std::is_same_v<Ty_, std::string>)  // if it's string, apply as-is.
                    parsed_json = env_value;
                else
                    parsed_json = nlohmann::json::parse(
                            env_value, env_value + strlen(env_value), nullptr, false);

                if (not parsed_json.is_discarded())
                    _opt->request_modify(std::move(parsed_json));
            }
    }

    config(config const&) noexcept = delete;
    config(config&&) noexcept = default;

    /**
     * @warning
     *    Reading Ty_ and invocation of update() MUST occur on same thread!
     *
     * @return
     */
    [[deprecated]] Ty_ const& get() const noexcept { return _value; }
    Ty_ value() const noexcept { return _copy(); }
    Ty_ const& ref() const noexcept { return _value; }

    /**
     * Provides thread-safe access for configuration.
     *
     * @return
     */
    Ty_ _copy() const noexcept { return _owner->_access_lock(), Ty_{_value}; }

    Ty_ const& operator*() const noexcept { return ref(); }
    Ty_ const* operator->() const noexcept { return &ref(); }
    operator Ty_() const noexcept { return _copy(); }

    [[deprecated]] bool check_dirty_and_consume() const { return _opt->consume_dirty(); }
    [[deprecated]] void async_modify(Ty_ v) { commit(std::move(v)); }
    bool check_update() const { return _opt->consume_dirty(); }
    void commit(Ty_ v) { _owner->bk_queue_update_value(_opt->full_key(), std::move(v)); }

    auto& base() const { return *_opt; }

   private:
    config_shared_ptr _opt;

    config_registry* _owner;
    Ty_ _value;
};

//! \see https://stackoverflow.com/questions/24855160/how-to-tell-if-a-c-template-type-is-c-style-string
template <typename Ty_, typename = void>
struct _cvt_ty_impl {
    using type = Ty_;
};

template <typename Ty_>
struct _cvt_ty_impl<Ty_, std::enable_if_t<std::is_same_v<std::decay_t<Ty_>, char const*>>> {
    using type = std::string;
};

template <typename Ty_>
struct _cvt_ty_impl<Ty_, std::enable_if_t<std::is_same_v<std::decay_t<Ty_>, char*>>> {
    using type = std::string;
};

template <typename Ty_>
using _cvt_ty = typename _cvt_ty_impl<std::decay_t<Ty_>>::type;

template <typename Ty_>
auto configure(config_registry& dispatcher,
               std::string&& full_key,
               Ty_&& default_value) noexcept
{
    _config_factory<_cvt_ty<Ty_>> attribute;
    attribute._pinfo = std::make_shared<typename _config_factory<_cvt_ty<Ty_>>::_init_info>();
    attribute._pinfo->dispatcher = &dispatcher;
    attribute._pinfo->default_value = std::forward<Ty_>(default_value);
    attribute._pinfo->full_key = std::move(full_key);

    return attribute;
}

}  // namespace perfkit
