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
#include "detail/configs-v2-frontend.hpp"
#include "detail/configs-v2.hpp"

#define INTL_PERFKIT_NS_0 ::perfkit::v2
#define INTL_PERFKIT_NS_1 ::perfkit::v2::_configs

#define PERFKIT_CFG_CLASS(ClassName) class ClassName : public INTL_PERFKIT_NS_0::config_set<ClassName>

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
    static inline const INTL_PERFKIT_NS_0::config_attribute_ptr                                   \
    INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName)                                   \
            = INTL_PERFKIT_NS_0::config_attribute_factory<decltype(VarName)::value_type>{         \
                    #VarName, INTL_PERFKIT_NS_1::name_from_va_arg(__VA_ARGS__)}                   \
                      ._internal_default_value(INTL_PERFKIT_NS_1::default_from_va_arg(__VA_ARGS__))

/**
 *
 */
#define PERFKIT_GCAT_ROOT(hierarchy)                 \
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
#define PERFKIT_GCFG_CAT_ROOT(hierarchy)                     \
    namespace hierarchy {                           \
    ::std::string _perfkit_INTERNAL_CATNAME_2();    \
    INTL_PERFKIT_NS_0::config_registry& registry(); \
    }                                               \
    namespace hierarchy

/**
 * Default config item definition macro
 */
#define PERFKIT_GCFG(VarName, DefaultValue, ... /*AttributeDecorators*/)                          \
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
#define PERFKIT_GCFG_CAT_ROOT_def(Namespace)                                                    \
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
#define PERFKIT_GCFG_CAT_def(Namespace)                                                            \
    namespace Namespace {                                                                       \
    static inline auto constexpr _perfkit_INTERNAL_CATNAME_BEFORE = &_perfkit_INTERNAL_CATNAME; \
                                                                                                \
    INTL_PERFKIT_NS_0::config_registry& registry()                                              \
    {                                                                                           \
        return _registry();                                                                     \
    }                                                                                           \
    ::std::string _perfkit_INTERNAL_CATNAME()                                                   \
    {                                                                                           \
        assert(_perfkit_INTERNAL_CATNAME_BEFORE != &_perfkit_INTERNAL_CATNAME);                 \
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
#define PERFKIT_GCFG_SUBSET(ClassName, VarName, ...) auto VarName = ClassName::_bk_create_va_arg(registry(), #VarName, ##__VA_ARGS__)

;  // namespace perfkit::v2
