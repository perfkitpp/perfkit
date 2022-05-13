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
#include "cpph/macros.hxx"
#include "detail/configs-v2-flags.hpp"
#include "detail/configs-v2.hpp"

namespace perfkit::v2 {
class config_set_base
{
   protected:
    struct _internal_initops_t {
        size_t property_offset;
        void (*fn_init)(config_set_base* base, void* prop_addr);
        void (*fn_deinit)(void* prop_addr);  // TODO: Logics for deactivation ...
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
    /**
     * Unload this config set from registered registry.
     *
     * @warning This method is not reentrant!
     */
    void force_deactivate()
    {
        for (auto& op : *_internal_initops()) {
            void* property = (char*)this + op.property_offset;
            op.fn_deinit(property);
        }
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

    static ImplType _bk_create_va_arg(config_registry_ptr existing, char const* memvar, char const* user_alias = nullptr)
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
        instance->activate(config_set_base::_internal_get_RG(b), move(prefix));
    };
    initop->fn_deinit = [](void* p) {
        ((Config*)p)->force_deactivate();
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
    initop->fn_deinit = [](void* p) {
        ((Subset*)p)->force_deactivate();
    };

    return nullptr;
}
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
}

#define INTL_PERFKIT_NS_0 ::perfkit::v2
#define INTL_PERFKIT_NS_1 ::perfkit::v2::_configs

#define PERFKIT_CFG(ClassName) class ClassName : public INTL_PERFKIT_NS_0::config_set<ClassName>

/**
 * Defines subset
 */
#define PERFKIT_CFG_SUBSET(ClassName, InstanceName, ...)                                                 \
    ClassName InstanceName = ClassName::_bk_make_subobj(                                                 \
            (INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, InstanceName)(), this),              \
            #InstanceName, ##__VA_ARGS__);                                                               \
                                                                                                         \
    static void INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, InstanceName)()                   \
    {                                                                                                    \
        static auto once = INTL_PERFKIT_NS_1::register_subset_function(&_internal_self_t::InstanceName); \
    }

/**
 * This defines attribute variable as static inline, which causes overhead on program startup.
 */
#define PERFKIT_CFG_ITEM(VarName, ...)                                                            \
    INTL_PERFKIT_NS_0::config<                                                                    \
            INTL_PERFKIT_NS_1::deduced_t<                                                         \
                    decltype(INTL_PERFKIT_NS_1::default_from_va_arg(__VA_ARGS__))>>               \
            VarName                                                                               \
            = {(INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, VarName)(),                \
                INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName))};                    \
                                                                                                  \
    static void INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, VarName)()                 \
    {                                                                                             \
        static auto once = INTL_PERFKIT_NS_1::register_conf_function(&_internal_self_t::VarName); \
    }                                                                                             \
                                                                                                  \
    static inline auto const INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName)          \
            = INTL_PERFKIT_NS_0::config_attribute_factory<decltype(VarName)::value_type>{         \
                    #VarName, INTL_PERFKIT_NS_1::name_from_va_arg(__VA_ARGS__)}                   \
                      ._internal_default_value(INTL_PERFKIT_NS_1::default_from_va_arg(__VA_ARGS__))

/**
 *
 */
#define PERFKIT_CFG_G_CAT(hierarchy)                 \
    namespace hierarchy {                            \
    ::std::string _perfkit_INTERNAL_CATNAME();       \
    ::std::string _perfkit_INTERNAL_CATNAME_2();     \
    INTL_PERFKIT_NS_0::config_registry& _registry(); \
    INTL_PERFKIT_NS_0::config_registry& registry();  \
    bool update();                                   \
    }                                                \
    namespace hierarchy

/**
 *
 */
#define PERFKIT_CFG_G_SUBCAT(hierarchy)             \
    namespace hierarchy {                           \
    ::std::string _perfkit_INTERNAL_CATNAME_2();    \
    INTL_PERFKIT_NS_0::config_registry& registry(); \
    }                                               \
    namespace hierarchy

/**
 * Default config item definition macro
 */
#define PERFKIT_CFG_ITEM_g(VarName, DefaultValue, ... /*AttributeDecorators*/)                         \
    static INTL_PERFKIT_NS_0::config_attribute_ptr                                                     \
            INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName)();                             \
                                                                                                       \
    auto VarName                                                                                       \
            = [] {                                                                                     \
                  INTL_PERFKIT_NS_0::config<INTL_PERFKIT_NS_1::deduced_t<decltype(DefaultValue)>>      \
                          tmp{(INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName)())};        \
                  ;                                                                                    \
                  tmp.activate(registry().shared_from_this(), _perfkit_INTERNAL_CATNAME_2());          \
                  return tmp;                                                                          \
              }();                                                                                     \
                                                                                                       \
    static INTL_PERFKIT_NS_0::config_attribute_ptr                                                     \
    INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName)()                                      \
    {                                                                                                  \
        static auto instance                                                                           \
                = INTL_PERFKIT_NS_0::config_attribute_factory<decltype(VarName)::value_type>{#VarName} \
                          ._internal_default_value(DefaultValue)                                       \
                                  __VA_ARGS__                                                          \
                          .confirm();                                                                  \
                                                                                                       \
        return instance;                                                                               \
    }

/**
 * Globally accessible repository. Place this somwhere of your program exactly once!
 */
#define PERFKIT_CFG_G_CAT_body(Namespace)                                                    \
    namespace Namespace {                                                                    \
    ::std::string _perfkit_INTERNAL_CATNAME()                                                \
    {                                                                                        \
        return "";                                                                           \
    }                                                                                        \
    ::std::string _perfkit_INTERNAL_CATNAME_2()                                              \
    {                                                                                        \
        return _perfkit_INTERNAL_CATNAME();                                                  \
    }                                                                                        \
    INTL_PERFKIT_NS_0::config_registry& _registry()                                          \
    {                                                                                        \
        static auto inst = INTL_PERFKIT_NS_0::config_registry::_internal_create(#Namespace); \
        return *inst;                                                                        \
    }                                                                                        \
    INTL_PERFKIT_NS_0::config_registry& registry()                                           \
    {                                                                                        \
        return _registry();                                                                  \
    }                                                                                        \
    }                                                                                        \
    namespace Namespace

/**
 * NEVER PLACE body after SUBCAT!
 */
#define PERFKIT_CFG_G_SUBCAT_body(Namespace)                                                    \
    namespace Namespace {                                                                       \
    static inline auto constexpr _perfkit_INTERNAL_CATNAME_BEFORE = &_perfkit_INTERNAL_CATNAME; \
                                                                                                \
    INTL_PERFKIT_NS_0::config_registry& registry()                                              \
    {                                                                                           \
        return _registry();                                                                     \
    }                                                                                           \
    ::std::string _perfkit_INTERNAL_CATNAME()                                                   \
    {                                                                                           \
        static_assert(_perfkit_INTERNAL_CATNAME_BEFORE != &_perfkit_INTERNAL_CATNAME);          \
        return Namespace::_perfkit_INTERNAL_CATNAME_BEFORE() + #Namespace "|";                  \
    }                                                                                           \
    ::std::string _perfkit_INTERNAL_CATNAME_2()                                                 \
    {                                                                                           \
        return _perfkit_INTERNAL_CATNAME();                                                     \
    }                                                                                           \
    }                                                                                           \
    namespace Namespace

/**
 * Create globally accessible category
 */
#define PERFKIT_CFG_G_SET(ClassName, VarName, ...) auto VarName = ClassName::_bk_create_va_arg(registry(), #VarName, ##__VA_ARGS__)

;  // namespace perfkit::v2
