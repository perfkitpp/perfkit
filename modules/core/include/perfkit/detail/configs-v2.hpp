/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2022. Seungwoo Kang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * project home: https://github.com/perfkitpp
 ******************************************************************************/

#pragma once
#include <utility>

#include "cpph/container/sorted_vector.hxx"
#include "cpph/hasher.hxx"
#include "cpph/refl/core.hxx"
#include "cpph/threading.hxx"
#include "nlohmann/json_fwd.hpp"

namespace perfkit::v2 {
class config_registry;
class config_base;
struct config_attribute;

using config_base_ptr = shared_ptr<config_base>;
using config_base_wptr = weak_ptr<config_base>;
using config_registry_ptr = shared_ptr<config_registry>;
using config_attribute_ptr = shared_ptr<config_attribute const>;

CPPH_UNIQUE_KEY_TYPE(config_id_t);
CPPH_UNIQUE_KEY_TYPE(config_registry_id_t);
CPPH_UNIQUE_KEY_TYPE(config_attribute_id_t);

//
using config_data = binary<string>;

enum class edit_mode : uint8_t {
    none,

    path = 10,
    path_file = 11,
    path_file_multi = 12,
    path_dir = 13,

    script = 20,  // usually long text

    color_b = 31,  // maximum 4 ch, 0~255 color range per channel
    color_f = 32,  // maximum 4 ch, usually 0.~1. range. Channel can exceed 1.
};

/**
 * Configs utility
 */
using config_registry_storage_t = std::map<std::string, nlohmann::json, std::less<>>;
using global_config_storage_t = std::map<std::string, config_registry_storage_t, std::less<>>;
void configs_dump_all(global_config_storage_t* json_dst);
bool configs_export_to(string_view path);
void configs_import_content(global_config_storage_t json_content);
bool configs_import_file(string_view path);

/**
 * Key rules
 *
 * example: +dsa32|MyCategory|SubRoutine|+1451|214.fdaso
 * - Tokens are separated by '|' character
 * - Any token starts with '+' character will be ignored, but referred by display order.
 */
struct config_attribute {
    // Unique id for single process instance.
    // Remote session can utilize this for attribute information caching.
    config_attribute_id_t unique_attribute_id;

    // Keys
    string name;  // Last token from full key chain

    //
    refl::shared_object_const_ptr default_value;

    // Creates empty object for various usage
    refl::shared_object_ptr (*fn_construct)(void);
    void (*fn_swap_value)(refl::shared_object_ptr, refl::shared_object_ptr);

    // Optional properties ...
    refl::shared_object_const_ptr one_of;
    refl::shared_object_const_ptr min;
    refl::shared_object_const_ptr max;

    // Validation functions
    function<bool(refl::object_view_t)> fn_validate;
    function<bool(refl::object_view_t)> fn_minmax_validate;

    //
    bool can_import = true;
    bool can_export = true;

    // Config source bindings
    vector<string> flag_bindings;
    string env_binding;

    // For remote session ...
    bool hidden = false;
    v2::edit_mode edit_mode;
    string description;
};

/**
 * If multiple consumer access same config instance, config update can be checked via this.
 *
 * @tparam Mutex
 */
template <typename Mutex = null_mutex>
class config_update_monitor
{
    sorted_vector<void const*, size_t> _table;
    Mutex _mtx;

   public:
    template <typename Config>
    bool check_update(Config&& conf)
    {
        auto fence = conf.fence();
        void const* key = conf._internal_unique_address();

        lock_guard _lc_{_mtx};
        auto prev_fence = &_table[key];

        if (*prev_fence != fence) {
            *prev_fence = fence;
            return true;
        } else {
            return false;
        }
    }

    void invalidate_all()
    {
        lock_guard _lc_{_mtx};
        _table.clear();
    }
};

/**
 * basic config class
 */
class config_base : public std::enable_shared_from_this<config_base>
{
    friend class config_registry;

   public:
    struct init_info_t {
        // Raw data pointer
        refl::shared_object_ptr raw_data;

        // Absolute
        config_attribute_ptr attribute;

        //
    };

   private:
    static inline std::atomic_uint64_t _idgen = 0;
    const config_id_t _id = {++_idgen};

    init_info_t _body;

    // Managed by repository
    string _cached_full_key;
    std::atomic_size_t _fence_modified = 0;  // Actual modification count

   public:
    explicit config_base(init_info_t&& info) noexcept : _body(move(info)) {}

    auto const& attribute() const noexcept { return _body.attribute; }
    auto const& default_value() const { return attribute()->default_value; }

    //! Returns config id. Id is unique for process scope.
    auto id() const { return _id; }

    auto const& name() const { return attribute()->name; }
    auto const& description() const { return attribute()->description; }

    size_t fence() const { return acquire(_fence_modified); }

    bool can_export() const noexcept { return attribute()->can_export; }
    bool can_import() const noexcept { return attribute()->can_import; }
    bool is_hidden() const noexcept { return attribute()->hidden; }
};

/*
 *
 */
class config_registry : public std::enable_shared_from_this<config_registry>
{
   public:
    class backend_t;
    using config_table = map<string_view, shared_ptr<config_base>>;
    using string_view_table = map<string_view, string_view>;
    using container = map<string, weak_ptr<config_registry>, std::less<>>;

   private:
    struct ctor_constraint_t {};

   private:
    shared_ptr<backend_t> _self;
    mutable size_t _fence_cached = 0;

   public:
    explicit config_registry(ctor_constraint_t, std::string name);
    ~config_registry() noexcept;

   public:
    //! Mark this registry as transient. It won't be exported.
    //! Call this before the first update() procedure call!
    void set_transient();

    //! Unmark this registry from transient state. Updates will be exported.
    void unset_transient();

    //! Manually unregister config registry.
    //! Useful when recreate registry immediately with same name
    bool unregister();

    //! Check if this registry is unregistered from global repository.
    bool is_registered() const;

    //! Check if this registry is transient
    bool is_transient() const;

    //! Flush queued changes.
    //! If it's first call to creation, it'll register itself to global repository.
    bool update();

    //! Export/Import
    void export_to(config_registry_storage_t* to, string* _ = nullptr) const;
    bool import_from(config_registry_storage_t const& from, string* _ = nullptr);

    //! Check if there was any update, without actual update() call.
    //! Return value is valid for each registry() instances.
    bool check_update() const
    {
        if (auto f = fence(); f != _fence_cached) {
            _fence_cached = f;
            return true;
        } else {
            return false;
        }
    }

    //! Name of this registery
    string const& name() const;

    //! adding and removing individual items won't trigger remote session update request
    //!  due to performance reasons. You must explicitly notify after adding series of
    //!  configuration items.
    void item_notify();

    //! Number of actual modification count.
    size_t fence() const;

    //! Id
    config_registry_id_t id() const noexcept;

   public:
    bool _internal_commit_value_user(config_base* ref, refl::shared_object_ptr);
    void const* _internal_unique_address() { return this; }

    //! Called once after creation.
    //! Just for reservation ...
    void _internal_init_registry() { (void)0; }

    //! Create unregistered config registry.
    static auto _internal_create(std::string name) -> shared_ptr<config_registry>
    {
        return make_shared<config_registry>(ctor_constraint_t{}, move(name));
    }

    //! Backend data provider
    auto* backend() const noexcept { return _self.get(); }

    //! Add item/remove item.
    //! All added item will be serialized to global context on first update after insertion
    void _internal_item_add(config_base_wptr arg, string prefix = "");
    void _internal_item_remove(config_base_wptr arg);

   public:
    //! Protects value from update
    void _internal_value_read_lock();
    void _internal_value_read_unlock();
    void _internal_value_write_lock();
    void _internal_value_write_unlock();
};

/**
 * 실제 사용자가 상호작용할 option 클래스
 *
 * 복사 가능. 복사된 인스턴스끼리는 update state를 공유하지 않는다.
 *
 */
template <typename ValueType>
class config
{
   public:
    using value_type = ValueType;

   private:
    shared_ptr<config_registry> _rg;
    config_base_ptr _base;

    ValueType const* _ref = nullptr;
    mutable size_t _update_check_fence = 0;

   public:
    config() noexcept = default;
    explicit config(config_attribute_ptr attrib)
    {
        // Retrieve default value from attribute
        auto default_value = refl::get_ptr<ValueType>(attrib->default_value);
        auto raw_data = make_shared<ValueType>(*default_value);

        _ref = raw_data.get();

        // Instantiate _base with attribute and default value
        config_base::init_info_t init;
        init.attribute = move(attrib);
        init.raw_data = raw_data;

        _base = make_shared<config_base>(move(init));
    }

   public:
    void commit(ValueType val) const
    {
        assert(_rg);
        _rg->_internal_commit_value_user(_base.get(), make_shared<ValueType>(move(val)));
    }

    ValueType value() const noexcept
    {
        assert(_rg);
        _rg->_internal_value_read_lock();
        ValueType value = *_ref;
        _rg->_internal_value_read_unlock();

        return value;
    }

    ValueType const& ref() const noexcept
    {
        return *_ref;
    }

    ValueType const* operator->() const noexcept
    {
        return _ref;
    }

    ValueType const& operator*() const noexcept
    {
        return *_ref;
    }

    operator ValueType() const noexcept
    {
        return value();
    }

    bool check_update() const noexcept
    {
        auto fence = _base->fence();
        return exchange(_update_check_fence, fence) != fence;
    }

   public:
    config_base_ptr base() const noexcept
    {
        return _base;
    }

    void activate(config_registry_ptr rg, string prefix = "")
    {
        force_deactivate();

        _rg.reset();
        _rg = move(rg);

        _rg->_internal_item_add(_base, move(prefix));
    }

    void force_deactivate()
    {
        if (_rg) {
            _rg->_internal_item_remove(_base);
            _rg.reset();
        }
    }

   public:
    void const* _internal_unique_address() const { return _ref; }
};

namespace _configs {
void parse_full_key(string const& full_key, string* o_display_key, vector<string_view>* o_hierarchy);
void verify_flag_string(string_view str);
}  // namespace _configs
}  // namespace perfkit::v2
