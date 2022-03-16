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

#pragma once
#include "perfkit/common/template_utils.hxx"
#include "perfkit/detail/configs.hpp"
#include "perfkit/fwd.hpp"

namespace perfkit::_configs_internal {
std::string INDEXER_STR(int order);
std::string INDEXER_STR2(int order);

//// no use anymore ... leaving just for reference
// template <typename TypeName_>
// static auto configure(
//         ::perfkit::config_registry* rg,
//         std::string name,
//         TypeName_&& default_value)
//         -> decltype(::perfkit::configure(
//                 std::declval<::perfkit::config_registry&>(),
//                 std::declval<std::string>(),
//                 std::declval<TypeName_>())) {
//   return ::perfkit::configure(
//           *rg,
//           std::move(name),
//           std::forward<TypeName_>(default_value));
// };

};  // namespace perfkit::_configs_internal

namespace perfkit {
struct config_class
{
    virtual ~config_class()                                          = default;
    virtual std::shared_ptr<perfkit::config_registry> _rg() noexcept = 0;
};

struct config_class_hook : std::function<void(config_class*)>
{
    using std::function<void(config_class*)>::function;
};
};  // namespace perfkit

#define INTERNAL_PERFKIT_STRINGFY_2(A) #A
#define INTERNAL_PERFKIT_STRINGFY(A)   INTERNAL_PERFKIT_STRINGFY_2(A)
#define INTERNAL_PERFKIT_INDEXER       "+" + ::perfkit::_configs_internal::INDEXER_STR(__COUNTER__) + "|"  // max 99999

#define INTERNAL_PERFKIT_CONFIG_COUNTER_NEXT(FieldName) \
    enum {                                              \
        _indexof_##FieldName = __LINE__                 \
    };

#define PERFKIT_FILEMARKER __FILE__ ":" INTERNAL_PERFKIT_STRINGFY(__LINE__) " " INTERNAL_PERFKIT_STRINGFY(__func__) "(): "

// HELPER MACROS
#define PERFKIT_CATEGORY(category)                                                       \
    namespace category {                                                                 \
    ::std::string _perfkit_INTERNAL_CATNAME()                                            \
    {                                                                                    \
        return "";                                                                       \
    }                                                                                    \
    ::std::string _perfkit_INTERNAL_CATNAME_2()                                          \
    {                                                                                    \
        return _perfkit_INTERNAL_CATNAME();                                              \
    }                                                                                    \
    ::perfkit::config_registry& _registry()                                              \
    {                                                                                    \
        static auto inst = ::perfkit::config_registry::create(#category);                \
        return *inst;                                                                    \
    }                                                                                    \
    [[deprecated]] ::perfkit::config_registry& root_registry() { return _registry(); }   \
    ::perfkit::config_registry&                registry() { return _registry(); }        \
                                                                                         \
    bool                                       update() { return _registry().update(); } \
    }                                                                                    \
    namespace category

#define PERFKIT_SUBCATEGORY(category)                                                                         \
    namespace category {                                                                                      \
    static inline auto                         _perfkit_INTERNAL_CATNAME_BEFORE = &_perfkit_INTERNAL_CATNAME; \
    [[deprecated]] ::perfkit::config_registry& root_registry() { return _registry(); }                        \
    ::perfkit::config_registry&                registry() { return _registry(); }                             \
    ::std::string                              _perfkit_INTERNAL_CATNAME()                                    \
    {                                                                                                         \
        return category::_perfkit_INTERNAL_CATNAME_BEFORE() + INTERNAL_PERFKIT_INDEXER + #category "|";       \
    }                                                                                                         \
    ::std::string _perfkit_INTERNAL_CATNAME_2()                                                               \
    {                                                                                                         \
        return _perfkit_INTERNAL_CATNAME();                                                                   \
    }                                                                                                         \
    }                                                                                                         \
    namespace category

#define PERFKIT_CONFIGURE(name, ...)                                                               \
    ::perfkit::config<::perfkit::_cvt_ty<decltype(__VA_ARGS__)>>                                   \
            name                                                                                   \
            = ::perfkit::configure(registry(),                                                     \
                                   _perfkit_INTERNAL_CATNAME_2() + INTERNAL_PERFKIT_INDEXER #name, \
                                   __VA_ARGS__)

#define PERFKIT_DECLARE_SUBCATEGORY(hierarchy)                                \
    namespace hierarchy {                                                     \
    ::std::string                              _perfkit_INTERNAL_CATNAME_2(); \
    [[deprecated]] ::perfkit::config_registry& root_registry();               \
    ::perfkit::config_registry&                registry();                    \
    }                                                                         \
    namespace hierarchy

#define PERFKIT_DECLARE_CATEGORY(hierarchy)                  \
    namespace hierarchy {                                    \
    ::std::string               _perfkit_INTERNAL_CATNAME(); \
    ::perfkit::config_registry& _registry();                 \
    ::perfkit::config_registry& registry();                  \
    bool                        update();                    \
    }                                                        \
    namespace hierarchy

#define INTERNAL_PERFKIT_T_CATEGORY_head(ClassName)

#define INTERNAL_PERFKIT_T_CATEGORY_tail(name)

#define INTERNAL_PERFKIT_T_CATEGORY_body(ClassName)                   \
   private:                                                           \
    using _internal_super = ClassName;                                \
    std::shared_ptr<::perfkit::config_registry> _perfkit_INTERNAL_RG; \
    static std::string                          _category_name()      \
    {                                                                 \
        return "";                                                    \
    }                                                                 \
                                                                      \
    std::shared_ptr<perfkit::config_registry> _rg() noexcept override \
    {                                                                 \
        return _perfkit_INTERNAL_RG;                                  \
    }                                                                 \
                                                                      \
   public:                                                            \
    ::perfkit::config_registry* operator->() { return _perfkit_INTERNAL_RG.get(); }

#define INTERNAL_PERFKIT_T_SUBCATEGORY_body(varname)                \
   private:                                                         \
    static std::string _category_name()                             \
    {                                                               \
        return _category_prev_##varname()                           \
             + ::perfkit::_configs_internal::INDEXER_STR2(__LINE__) \
             + #varname "|";                                        \
    }                                                               \
                                                                    \
    ::perfkit::config_registry* _perfkit_INTERNAL_RG;               \
                                                                    \
   public:                                                          \
    template <typename TT_>                                         \
    explicit varname##_(TT_* _super)                                \
            : _perfkit_INTERNAL_RG(&*_super->_perfkit_INTERNAL_RG)  \
    {                                                               \
    }                                                               \
                                                                    \
   private:                                                         \
    using _internal_super = varname##_;                             \
                                                                    \
   public:

#define PERFKIT_T_CATEGORY(varname, ...)                                             \
    struct varname : ::perfkit::config_class                                         \
    {                                                                                \
        INTERNAL_PERFKIT_T_CATEGORY_body(varname);                                   \
                                                                                     \
        explicit varname(std::string s) : _perfkit_INTERNAL_RG(                      \
                ::perfkit::config_registry::create(std::move(s), &typeid(*this))) {} \
                                                                                     \
        __VA_ARGS__;                                                                 \
    }

#define PERFKIT_T_SUBCATEGORY(varname, ...)                                    \
    static std::string _category_prev_##varname() { return _category_name(); } \
    struct varname##_                                                          \
    {                                                                          \
        INTERNAL_PERFKIT_T_SUBCATEGORY_body(varname);                          \
        __VA_ARGS__;                                                           \
    } varname{this};

#define PERFKIT_T_CONFIGURE(ConfigName, ...)                                       \
    ::perfkit::config<                                                             \
            ::perfkit::_cvt_ty<decltype(__VA_ARGS__)>>                             \
            ConfigName = ::perfkit::configure(                                     \
                    *_perfkit_INTERNAL_RG,                                         \
                    _category_name()                                               \
                            + ::perfkit::_configs_internal::INDEXER_STR2(__LINE__) \
                            + #ConfigName,                                         \
                    __VA_ARGS__)

#define PERFKIT_T_EXPAND_CATEGORY(varname, ...)          \
    struct varname : ::perfkit::config_class             \
    {                                                    \
        INTERNAL_PERFKIT_T_CATEGORY_body(varname);       \
        explicit varname(::perfkit::config_class* other) \
                : _perfkit_INTERNAL_RG(other->_rg()) {}  \
                                                         \
        __VA_ARGS__;                                     \
    };
