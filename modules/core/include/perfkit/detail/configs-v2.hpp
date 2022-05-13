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

namespace perfkit::configs_v2 {
class config_registry_proxy;
class config_registry_body;
class config_base;
struct config_attribute;

using config_base_ptr = shared_ptr<config_base>;
using config_base_wptr = weak_ptr<config_base>;
using config_registry_ptr = shared_ptr<config_registry_proxy>;
using config_raw_data_ptr = unique_ptr<void, void (*)(void*)>;
using config_attribute_ptr = shared_ptr<config_attribute const>;

using config_data = binary<string>;

enum class edit_mode {
    none,

    path = 10,
    path_file,
    path_file_multi,
    path_dir,

    script,  // usually long text

    color_b,  // maximum 4 ch, 0~255 color range per channel
    color_f,  // maximum 4 ch, usually 0.~1. range. Channel can exceed 1.
};

struct config_attribute {
    refl::object_metadata_t metadata;

    config_data default_value;
    string description;

    config_data one_of;
    config_data min;
    config_data max;

    //
    bool has_custom_validator = false;

    bool can_import = true;
    bool can_export = true;

    bool has_flag = false;
    bool hidden = false;

    //
    vector<std::string> flag_bindings;

    // Keys
    string full_key;
    string display_key;

    // Array view hierarchy
    vector<string_view> hierarchy;
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
 *
 * TODO: Attribute retrieval for config class
 */
class config_base : public std::enable_shared_from_this<config_base>
{
   public:
    struct context_t {
        // Raw data pointer
        config_raw_data_ptr raw_data;

        // Unique key in registry scope
        std::string prefix;

        // Absolute
        config_attribute_ptr attribute;
    };

   private:
    context_t _context;

    std::atomic_bool _latest_marshal_failed = false;

    std::atomic_size_t _fence_modified = 0;            // Actual modification count
    std::atomic_size_t _fence_modify_requested = 0;    // Modification request count
    std::atomic_size_t _fence_serialized = ~size_t{};  // Serialization is dirty when _fence_modify_requested != _fence_serialized

   public:
    explicit config_base(context_t&& info);

    auto const& attribute() const noexcept { return _context.attribute; }
    auto const& default_value() const { return attribute()->default_value; }

    auto const& full_key() const { return attribute()->full_key; }
    auto const& display_key() const { return attribute()->display_key; }
    auto const& description() const { return attribute()->description; }
    auto tokenized_display_key() const { return make_view(attribute()->hierarchy); }

    size_t num_modified() const { return acquire(_fence_modified); };
    size_t num_serialized() const { return _fence_serialized; }

    bool can_export() const noexcept { return attribute()->can_export; }
    bool can_import() const noexcept { return attribute()->can_import; }
    bool is_hidden() const noexcept { return attribute()->hidden; }

    /**
     * Check if latest marshalling result was invalid
     * @return
     */
    bool latest_marshal_failed() const
    {
        return _latest_marshal_failed.load(std::memory_order_relaxed);
    }
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
 * TODO: 레거시 호환 Macro 정의 시 ...
 *    #define PERFKIT_T_CONFIGURE(VarName, ...)
 *      config<type_from_default<decltype(__VA_ARGS__)>> VarName{
 *          *_internal_RG, INTERNAL_PERFKIT_CONCAT(_attr_, VarName), FULL_KEY_GENERATED, __VA_ARGS__ };
 *      static inline auto const INTERNAL_PERFKIT_CONCAT(_register_, VarName)
 *          = [this] {
 *              using self_t = decay_t<decltype(*this)>;
 *              _internal_initops()->emplace_back(
 *                  offsetof(self_t, VarName),
 *                  [](config_registry_proxy*, void* ptr) {
 *
 *                  }
 *              );
 *          };
 *      static inline auto const INTERNAL_PERFKIT_CONCAT(_attr_, VarName)
 *          = ::perfkit::config_details::attribute_factory{}
 *  USAGE:
 *    PERFKIT_T_CONFIGURE(MY_NAME, 3.314).confirm();
 *
 * TODO:
 *  PERFKIT_T_CATEGORY 생성자 변경 ->
 *      - Create with string: New instance
 *      - Create with existing registry + string: Append to existing registry with prefix
 *
 * class MyConfigCategory : public perfkit::_config::category<MyConfigCategory>
 * {
 *      using perfkit::_config::category::category;
 *
 *      class MySubCategory : public perfkit::_config::category<MySubCategory>
 *      {
 *          using perfkit::_config::category::category;
 *      };
 *
 *      MySubCategory subc1{this};
 *      MySubCategory subc2{this};
 *
 *
 * };
 *
 * PERFKIT_CONFIG_CATEGORY(MyCategoryName)
 * {
 *      PERFKIT_CONFIG_CATEGORY(MySubCategoryType)
 *      {
 *      }; // Linked list로 서브카테고리 리스트 관리 ...
 *         // super->last->next = this, super->last = this
 *
 *      PERFKIT_CONFIG_SUBCATEGORY_ITEM(MySubCategoryType, my_sub_category);
 *
 *      PERFKIT_CONFIG_ITEM(my_entity, 3.315)
 *          .min(0)
 *          .max(1.4)
 *          .flags("f,g,flow-control")
 *          .description("hell, world!")
 *          .confirm();
 *
 *      PERFKIT_CONFIG_ITEM(my_array, array<int, 3>{})
 *          .edit_mode(perfkit::edit_mode::color_b) // 0~ 255 per channel
 *          .confirm();
 *
 *      PERFKIT_CONFIG_ITEM(my_array, array<float, 3>{})
 *          .edit_mode(perfkit::edit_mode::color_f) // 0.~ 1.~ per channel
 *          .confirm();
 *
 *      PERFKIT_CONFIG_ITEM(my_pat, "")
 *          .edit_mode(edit_mode::path_file) // path_file_multi, path_dir, path_dir_multi
 *          .confirm();
 *
 *      PERFKIT_CONFIG_ITEM(my_script, "...")
 *          .edit_mode(edit_mode::script) // path_file_multi, path_dir, path_dir_multi
 *          .confirm();
 * };
 *
 *
 *
 * perfkit::_config::category<MyConfigCategory>
 *   -> static MyConfigCategory create(key_str, ...) 정의
 *   -> static MyConfigCategory create(registry, prefix_str, ...) 정의
 *   -> static  정의
 *
 */
class config_registry_proxy : public std::enable_shared_from_this<config_registry_proxy>
{
   public:
    using config_table = map<string_view, shared_ptr<config_base>>;
    using string_view_table = map<string_view, string_view>;
    using container = map<string, weak_ptr<config_registry_proxy>, std::less<>>;

   private:
   private:
    shared_ptr<config_registry_body> _data;

   private:
    explicit config_registry_proxy(std::string name);

   public:
    ~config_registry_proxy() noexcept;

   public:
    //! Mark this registry as transient. It won't be exported.
    void set_transient() {}

    //! Unmark this registry from transient state. Updates will be exported.
    void unset_transient() {}

    //! Manually unregister config registry.
    //! Useful when
    bool unregister() { return false; }

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
    static auto _internal_create(std::string name) -> shared_ptr<config_registry_proxy>;

    void bk_find_key(string_view display_key, string* out_full_key) {}
    void bk_all(vector<config_base_ptr>*) const noexcept {}

    auto& bk_data() const noexcept { return _data; }

   public:
    //! Protects value from update
    void _internal_value_access_lock() {}
    void _internal_value_access_unlock() {}

   public:
    static void bk_enumerate_registries(vector<shared_ptr<config_registry_proxy>>* out_regs, bool filter_complete = false) noexcept {}
    static auto bk_find_registry(string_view name) noexcept -> shared_ptr<config_registry_proxy> { return {}; }
};

/**
 * 실제 사용자가 상호작용할 option 클래스
 *
 * TODO: 복사 가능. 복사된 인스턴스끼리는 update state를 공유하지 않는다.
 *
 */
template <typename ValueType>
class config
{
    shared_ptr<config_registry_proxy> _owner;
    config_base_ptr _base;

    ValueType const* _ref = nullptr;
    size_t _update_check_fence = 0;

   private:
    // TODO: friend factory class ...
    struct construction_constraint_t {};

   public:
    explicit config(construction_constraint_t) {}

   public:
    // TODO: ... getters / commit / check_update ...
    void commit(ValueType const& val)
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

    bool check_update() noexcept
    {
        auto fence = _base->num_modified();
        return exchange(_update_check_fence, fence) != fence;
    }

   public:
    config_base_ptr base() const noexcept
    {
        return _base;
    }

   public:
    void const* _internal_unique_address() const { return _ref; }
};

namespace _configs {
class config_set_base
{
   protected:
    struct initops_t {
        size_t property_offset;
        void (*fn_init)(config_set_base* base, void* prop_addr);
    };

   protected:
    shared_ptr<config_registry_proxy> _internal_RG;
    string _internal_prefix;

   public:
    static auto _internal_initops() -> vector<initops_t>*
    {
        static vector<initops_t> _inst;
        return &_inst;
    }

   protected:
    static void _internal_perform_initops(config_set_base* base_addr, array_view<initops_t const> initops)
    {
        //! Perform init ops
        for (auto& op : initops) {
            void* property = (char*)base_addr + op.property_offset;
            op.fn_init(base_addr, property);
        }
    }

    static auto _internal_retrieve_RG(config_set_base* b) { return b->_internal_RG; }
};

template <typename ImplType>
class config_set : public config_set_base
{
   protected:
    using _internal_self_t = ImplType;

   public:
    //! May throw logic_error on registry creation fails.
    //! Create new config registry
    static ImplType create(string key)
    {
        ImplType s;
        s._internal_RG = config_registry_proxy::_internal_create(move(key));
        s._internal_RG->_internal_register_to_global();

        _internal_perform_initops(&s, *_internal_initops());
        return s;
    }

    //! Append this category to existing registry.
    static ImplType create(config_registry_ptr existing, string prefix = "")
    {
        ImplType s;
        s._internal_RG = move(existing);
        s._internal_prefix = move(prefix);

        _internal_perform_initops(&s, *_internal_initops());
        return s;
    }

   public:
    //! Only for category subprocess usage ...
    static ImplType _bk_make_subobj(config_set_base* base, string_view memvar_name, string_view user_provide = "")
    {
        ImplType s;
        s._internal_RG = _internal_retrieve_RG(base);
        s._internal_prefix = user_provide.empty() ? memvar_name : user_provide;

        // Do not perform initops on construction.
        // Initops will be deferred until superset initops iteration
        // _internal_perform_initops(&s, *_internal_initops());
        return s;
    }

   public:
    config_registry_proxy& operator*() const noexcept { return *_internal_RG; }
    config_registry_proxy* operator->() const noexcept { return _internal_RG.get(); }
};

template <class ObjClass, typename ValTy>
size_t offset_of(ValTy ObjClass::*mptr)
{
    return reinterpret_cast<size_t>(&(reinterpret_cast<ObjClass const volatile*>(0)->*mptr));
}

template <typename ConfigSet, typename Config>
nullptr_t register_conf_function(Config ConfigSet::*mptr)
{
    auto storage = ConfigSet::_internal_initops();
    auto initop = &storage->emplace_back();
    initop->property_offset = offset_of(mptr);
    initop->fn_init = [](config_set_base* b, void* p) {
        auto instance = (Config*)p;

        // TODO: Register config instance to registry
    };

    return nullptr;
}

template <typename ConfigSet, typename Subset>
nullptr_t register_subset_function(Subset ConfigSet::*mptr)
{
    auto storage = ConfigSet::_internal_initops();
    auto initop = &storage->emplace_back();
    initop->property_offset = offset_of(mptr);
    initop->fn_init = [](config_set_base* b, void* p) {
        auto instance = (Subset*)p;

        // Perform subset initops here
    };

    return nullptr;
}

#if 1
class MyConf : public config_set<MyConf>
{
    int rgg = _internal_perfkit_attribute_rgg;
    static inline auto const _internal_perfkit_register_conf_rgg
            = register_conf_function(&_internal_self_t::rgg);
    static inline auto const _internal_perfkit_attribute_rgg
            = 3;

    class MySubConf : public config_set<MySubConf>
    {
        int rgg = _internal_perfkit_attribute;
        static inline auto const _internal_perfkit_register_conf_rgg
                = register_conf_function(&_internal_self_t::rgg);
        static inline auto const _internal_perfkit_attribute
                = 3;
    };

    // #define PERFKIT_CFG_SUBSET(SetType, VarName, ...) ...(this, #VarName, ##__VA_ARGS__) ...
    MySubConf myconf = MySubConf::_bk_make_subobj(this, "myconf", "MYSUBCONF");
    static inline auto const _internal_perfkit_register_conf_myconf
            = register_subset_function(&_internal_self_t::myconf);
};

void foof()
{
    auto r = MyConf::create("hell");
}
#endif

}  // namespace _configs

}  // namespace perfkit::configs_v2
