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
#include "cpph/container/sorted_vector.hxx"
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

struct config_attribute {
    // Unique id for single process instance.
    // Remote session can utilize this for attribute information caching.
    uint64_t unique_attribute_id;

    // Keys
    string full_key_chain;  // May contain multiple tokens
    string_view name;       // Last token from full key chain

    //
    refl::shared_object_ptr default_value;

    // Optional properties ...
    refl::shared_object_ptr one_of;
    refl::shared_object_ptr min;
    refl::shared_object_ptr max;

    // Validation functions
    function<bool(refl::object_view_t)> fn_validate;
    function<bool(refl::object_const_view_t)> fn_verify;

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
 * If multiple consumer access same config instance,
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
   public:
    struct context_t {
        // Raw data pointer
        refl::shared_object_ptr raw_data;

        // Absolute
        config_attribute_ptr attribute;
    };

   private:
    std::atomic_uint64_t _idgen = 0;
    const uint64_t _id = ++_idgen;
    context_t _context;

    std::atomic_bool _latest_marshal_failed = false;
    std::atomic_size_t _fence_modified = 0;            // Actual modification count
    std::atomic_size_t _fence_modify_requested = 0;    // Modification request count
    std::atomic_size_t _fence_serialized = ~size_t{};  // Serialization is dirty when _fence_modify_requested != _fence_serialized

   public:
    explicit config_base(context_t&& info) noexcept : _context(move(info)) {}

    auto const& attribute() const noexcept { return _context.attribute; }
    auto const& default_value() const { return attribute()->default_value; }

    //! Returns config id. Id is unique for process scope.
    auto id() const { return _id; }

    auto const& name() const { return attribute()->name; }
    auto const& description() const { return attribute()->description; }

    size_t num_modified() const { return acquire(_fence_modified); };
    size_t num_serialized() const { return _fence_serialized; }

    bool can_export() const noexcept { return attribute()->can_export; }
    bool can_import() const noexcept { return attribute()->can_import; }
    bool is_hidden() const noexcept { return attribute()->hidden; }

    void get_keys_by_token(vector<string_view>* out) {}
    void get_full_key(string* out) {}  // TODO: Recursively refer _host, and construct full key.

    /**
     * Check if latest marshalling result was invalid
     * @return
     */
    bool latest_marshal_failed() const { return acquire(_latest_marshal_failed); }

   public:
};

/*
 * TODO: PERFKIT_T_CATEGORY 는 여전히 self-contained.
 *  단 PERFKIT_CONFIG_TEMPLATE 클래스 추가,
 *
 * TODO: 요구사항
 *  - Config Registry에는 시점에 관계없이 자유롭게 config 인스턴스를 추가/제거 가능
 *  - Config registry는 각 config_base의 weak reference만 물고 있음
 *  - config_base는 죽을 때 registry에 notify ... 곧바로 클라이언트에 전파 (등록 시에도 마찬가지)
 *
 * TODO: 레거시 호환
 *  - PREFKIT_CONFIGURE 등 모든 config 매크로 호환되어야 함
 *  - config.h include 시 호환성 문제 없어야 함
 *
 *  USAGE:
 *    PERFKIT_T_CONFIGURE(MY_NAME, 3.314).confirm();
 *
 */
class config_registry : public std::enable_shared_from_this<config_registry>
{
   public:
    using config_table = map<string_view, shared_ptr<config_base>>;
    using string_view_table = map<string_view, string_view>;
    using container = map<string, weak_ptr<config_registry>, std::less<>>;

   private:
   private:
    class backend_t;
    shared_ptr<backend_t> _self;

   private:
    explicit config_registry(std::string name) {}

   public:
    ~config_registry() noexcept {}

   public:
    //! Mark this registry as transient. It won't be exported.
    //! Call this before first update() procedure call!
    void set_transient() {}

    //! Unmark this registry from transient state. Updates will be exported.
    void unset_transient() {}

    //! Manually unregister config registry.
    //! Useful when recreate registry immediately with same name
    bool unregister() { return false; }

    //! Check if this registry is unregistered from global repository.
    bool is_registered() const { return false; }

    //! Flush queued changes to individual
    bool update() { return false; }
    void export_to(archive::if_writer*) {}
    void import_from(archive::if_reader*) {}

    //! Name of this registery
    string_view name() const { return ""; }

    //! Add item/remove item.
    //! All added item will be serialized to global context on first update after insertion
    void item_add(config_base_ptr arg) {}
    void item_remove(config_base_wptr arg) {}

    //! adding and removing individual items won't trigger remote session update request
    //!  due to performance reasons. You must explicitly notify after adding series of
    //!  configuration items.
    void item_notify() {}

    //! Retrieve serialized data from config
    void retrieve_serialized_data(config_base_ptr const&, config_data* out) {}

   public:
    bool _internal_commit_value(config_base_wptr ref, refl::object_const_view_t) { return false; }
    void const* _internal_unique_address() { return this; }

    //! Register config registry to global repository
    //! Global configs registry update will be deferred until first update() of this class.
    void _internal_register_to_global() {}

    //! Create unregistered config registry.
    static auto _internal_create(std::string name) -> shared_ptr<config_registry>;

    //! Backend data provider
    auto* backend() const noexcept { return _self.get(); }

   public:
    //! Protects value from update
    void _internal_value_access_lock() {}
    void _internal_value_access_unlock() {}
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
    shared_ptr<config_registry> _owner;
    config_base_ptr _base;

    ValueType const* _ref = nullptr;
    mutable size_t _update_check_fence = 0;

   public:
    config() noexcept = default;
    config(config_attribute_ptr attrib)
    {
    }

   public:
    void commit(ValueType const& val) const
    {
        _owner->_internal_commit_value(_base, {val});
    }

    ValueType value() const noexcept
    {
        _owner->_internal_value_access_lock();
        ValueType value = *_ref;
        _owner->_internal_value_access_unlock();

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
        auto fence = _base->num_modified();
        return exchange(_update_check_fence, fence) != fence;
    }

   public:
    config_base_ptr base() const noexcept
    {
        return _base;
    }

    void register_to(config_registry_ptr rg, string prefix = "")
    {
        if (_owner) {
            _owner->item_remove(_base);
        }

        _owner.reset();
        _owner = rg;

        _owner->item_add(_base);
    }

   public:
    void const* _internal_unique_address() const { return _ref; }
};

namespace _configs {
void parse_full_key(string const& full_key, string* o_display_key, vector<string_view>* o_hierarchy);
void verify_flag_string(string_view str);

}  // namespace _configs

template <typename ValTy>
class config_attribute_factory
{
   public:
    using self_reference = config_attribute_factory&;

   private:
    shared_ptr<config_attribute> _ref;

   public:
    //! Full key will automatically be parsed into display keys
    explicit config_attribute_factory(string key, string_view user_alias = "") noexcept : _ref(make_shared<config_attribute>())
    {
        static std::atomic_uint64_t _idgen = 0;
        _ref->name = user_alias.empty() ? move(key) : user_alias;
        _ref->unique_attribute_id = ++_idgen;
    }

    //! Sets default value. Will automatically be called internal
    self_reference _internal_default_value(ValTy value) noexcept
    {
        _ref->default_value.reset(make_shared<ValTy>(move(value)));
        return *this;
    }

   public:
    /** Description to this property */
    self_reference description(string content) noexcept
    {
        _ref->description = move(content);
        return *this;
    }

    /** Editing type specifier. */
    self_reference edit_mode(edit_mode mode) noexcept
    {
        assert(mode != edit_mode::none);
        assert(_ref->edit_mode == edit_mode::none);

        _ref->edit_mode = mode;
        return *this;
    }

    /** Value bottom limit */
    self_reference min(ValTy value) noexcept
    {
        assert(_ref->min == nullptr);
        _ref->min.reset(make_shared<ValTy>(move(value)));
        return *this;
    }

    /** Value maximum limit */
    self_reference max(ValTy value) noexcept
    {
        assert(_ref->max == nullptr);
        _ref->max.reset(make_shared<ValTy>(move(value)));
        return *this;
    }

    /** minmax Helper */
    self_reference clamp(ValTy minv, ValTy maxv) noexcept
    {
        assert(std::less<>{}(minv, maxv));
        return min(move(minv)), max(move(maxv));
    }

    /**
     * Only one of the given elements can be selected.
     */
    template <typename Iterable>
    self_reference one_of(Iterable&& iterable) noexcept
    {
        auto values = make_shared<vector<ValTy>>();

        for (auto&& e : iterable)
            values->emplace_back(forward<decltype(e)>(e));

        _ref->one_of.reset(values);
        _ref->fn_validate
                = [values = values.get()](refl::object_const_view_t v) {
                      auto value = refl::get_ptr<ValTy>(v);
                      assert(value);

                      auto iter = find(*values, *value);
                      return iter != values->end();
                  };

        return *this;
    }

    // Overloaded version of one_of for initializer_list
    self_reference one_of(initializer_list<ValTy> v) noexcept
    {
        return one_of(array_view{&v.begin()[0], v.size()});
    }

    /**
     * Hidden element hint. Hidden element commonly won't be displayed to terminal remote endpoint.
     */
    self_reference hide() noexcept
    {
        _ref->hidden = true;
        return *this;
    }

    /**
     * validate function makes value assignable to destination.
     * if callback returns false, change won't be applied (same as verify())
     */
    template <typename Pred>
    auto& validate(Pred&& pred) noexcept
    {
        assert(not _ref->fn_validate);

        if constexpr (std::is_invocable_r_v<bool, Pred, ValTy&>) {
            _ref->fn_validate
                    = [pred = forward<Pred>(pred)](refl::object_view_t v) {
                          return pred(refl::get<ValTy>(v));
                      };
        } else if constexpr (std::is_invocable_v<Pred, ValTy&>) {
            _ref->fn_validate
                    = [pred = forward<Pred>(pred)](refl::object_view_t v) {
                          pred(refl::get<ValTy>(v));
                          return true;
                      };
        } else {
            pred->STATIC_ERROR_INVALID_CALLABLE;
        }

        return *this;
    }

    /** verify function will discard updated value on verification failure. */
    template <typename Pred, typename = enable_if_t<is_invocable_r_v<bool, Pred, ValTy const&>>>
    auto& verify(Pred&& pred) noexcept
    {
        assert(not _ref->fn_verify);

        _ref->fn_verify
                = [pred = forward<Pred>(pred)](refl::object_const_view_t v) {
                      return pred(refl::get<ValTy const>(v));
                  };

        return *this;
    }

    /** expose entities as flags. configs MUST NOT BE field of any config template class! */
    template <typename... Str_>
    auto& flags(Str_&&... args) noexcept
    {
        static_assert(sizeof...(args) > 0);
        assert(_ref->flag_bindings.empty());

        vector<string> flags;
        auto fn_put_elem =
                [&](auto&& elem) {
                    using elem_type = decay_t<decltype(elem)>;
                    if constexpr (is_same_v<char, elem_type>)
                        flags.emplace_back(elem, 1);
                    else
                        flags.emplace_back(forward<decltype(elem)>(elem));
                };

        (fn_put_elem(std::forward<Str_>(args)), ...);

        _ref->flag_bindings = move(flags);
        return *this;
    }

    /** transient marked configs won't be saved or loaded from config files. */
    auto& transient() noexcept
    {
        assert(_ref->can_export && _ref->can_import);  // Maybe there was readonly() or another transient() call?
        _ref->can_import = _ref->can_export = false;
    }

    /** readonly marked configs won't be exported, but still can be imported from config files. */
    auto& readonly() noexcept
    {
        assert(_ref->can_export && _ref->can_import);  // Maybe there was readonly() or another transient() call?
        _ref->can_import = true;
        _ref->can_export = false;
    }

    /** initialize from environment variable */
    auto& env(string s) noexcept
    {
        assert(_ref->env_binding.empty());
        _ref->env_binding = move(s);
    }

   public:
    /** Confirm attribute instance creation */
    auto confirm() noexcept
    {
        fflush(stdout);
        return move(_ref);
    }
};

namespace _configs {
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
using deduced_t = typename _cvt_ty_impl<std::decay_t<Ty_>>::type;

template <typename RawVal>
auto default_from_va_arg(RawVal&& v, char const* msg = nullptr) -> deduced_t<RawVal>
{
    return {v};
}
template <typename RawVal>
char const* name_from_va_arg(RawVal&&, char const* name = "")
{
    return name;
}
}  // namespace _configs

class config_set_base
{
   protected:
    struct _internal_initops_t {
        size_t property_offset;
        void (*fn_init)(config_set_base* base, void* prop_addr);
    };

    struct _internal_pfx_node {
        string content;
        weak_ptr<_internal_pfx_node> parent;

        _internal_pfx_node(string content, weak_ptr<_internal_pfx_node> parent = {})
                : content(move(content)), parent(parent) {}
    };

   protected:
    static thread_local inline shared_ptr<config_registry> _internal_RG_next;
    static thread_local inline shared_ptr<_internal_pfx_node> _internal_prefix_next;

    shared_ptr<config_registry> _internal_RG = exchange(_internal_RG_next, nullptr);
    shared_ptr<_internal_pfx_node> _internal_prefix = exchange(_internal_prefix_next, nullptr);

   public:
    static void _internal_perform_initops(config_set_base* base_addr, array_view<_internal_initops_t const> initops)
    {
        //! Perform init ops
        for (auto& op : initops) {
            void* property = (char*)base_addr + op.property_offset;
            op.fn_init(base_addr, property);
        }
    }

   public:
    static auto _internal_get_RG(config_set_base* b) { return b->_internal_RG; }
    static auto const& _internal_get_prefix(config_set_base* b) { return b->_internal_prefix; }
};

template <typename ImplType>
class config_set : public config_set_base
{
   protected:
    using _internal_self_t = ImplType;

   public:
    static auto _internal_initops() -> vector<_internal_initops_t>*
    {
        // Contains information for registering properties / subset initializations
        static vector<_internal_initops_t> _inst;
        return &_inst;
    }

   public:
    //! May throw logic_error on registry creation fails.
    //! Create new config registry
    static ImplType create(string key)
    {
        config_set_base::_internal_RG_next = config_registry::_internal_create(move(key));
        ImplType s;

        _internal_perform_initops(&s, *_internal_initops());
        s._internal_RG->_internal_register_to_global();
        return s;
    }

    //! Append this category to existing registry.
    static ImplType create(config_registry_ptr existing, string prefix = "")
    {
        config_set_base::_internal_RG_next = move(existing);
        ImplType s;

        if (not prefix.empty()) {
            s._internal_prefix = make_shared<_internal_pfx_node>(move(prefix));
        }

        _internal_perform_initops(&s, *_internal_initops());
        return s;
    }

   public:
    //! Only for category subprocess usage ...
    static ImplType _bk_make_subobj(config_set_base* base, char const* memvar, char const* user_provided = nullptr)
    {
        auto base_prefix = config_set_base::_internal_get_prefix(base);

        config_set_base::_internal_RG_next = _internal_get_RG(base);
        config_set_base::_internal_prefix_next = make_shared<_internal_pfx_node>(
                user_provided ? user_provided : memvar, base_prefix);
        ImplType s;

        // Do not perform initops on construction.
        // Initops will be deferred until superset initops iteration
        // - _internal_perform_initops(&s, *_internal_initops());
        return s;
    }

    static ImplType _bk_create_global(config_registry_ptr existing, char const* memvar, char const* user_alias = nullptr)
    {
        return create(move(existing), user_alias ? user_alias : memvar);
    }

   public:
    config_registry& operator*() const noexcept { return *_internal_RG; }
    config_registry* operator->() const noexcept { return _internal_RG.get(); }
};

namespace _configs {
template <class ObjClass, typename ValTy>
size_t offset_of(ValTy ObjClass::*mptr)
{
    return reinterpret_cast<size_t>(&(reinterpret_cast<ObjClass const volatile*>(0)->*mptr));
}

template <typename ConfigSet, typename Config>
nullptr_t register_conf_function(Config ConfigSet::*mptr) noexcept
{
    auto storage = ConfigSet::_internal_initops();
    auto initop = &storage->emplace_back();
    initop->property_offset = offset_of(mptr);
    initop->fn_init = [](config_set_base* b, void* p) {
        auto instance = (Config*)p;
        config_base_ptr base = instance->base();

        // Build prefix string
        string prefix;
        for (auto node = config_set_base::_internal_get_prefix(b); node;) {
            auto const& pfx = node->content;
            prefix.reserve(prefix.size() + 1 + pfx.size());
            prefix += '|';
            prefix.append(pfx.rbegin(), pfx.rend());

            node = node->parent.lock();
        }
        reverse(prefix);

        // Register config instance to registry
        instance->register_to(config_set_base::_internal_get_RG(b), move(prefix));
    };

    return nullptr;
}

template <typename ConfigSet, typename Subset>
nullptr_t register_subset_function(Subset ConfigSet::*mptr) noexcept
{
    auto storage = ConfigSet::_internal_initops();
    auto initop = &storage->emplace_back();
    initop->property_offset = offset_of(mptr);
    initop->fn_init = [](config_set_base*, void* p) {
        auto instance = (Subset*)p;

        // Perform subset initops here
        Subset::_internal_perform_initops(instance, *Subset::_internal_initops());
    };

    return nullptr;
}
}  // namespace _configs
}  // namespace perfkit::v2
