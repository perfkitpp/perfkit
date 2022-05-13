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
 * Default config item definition macro
 */
#define PERFKIT_CFG_ITEM_LAZY(VarName, DefaultValue, UserKey /*={}*/, ... /*AttributeDecorators*/)               \
    INTL_PERFKIT_NS_0::config<INTL_PERFKIT_NS_1::deduced_t<decltype(DefaultValue)>> VarName                      \
            = {(INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, VarName)(),                               \
                INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName)())};                                 \
                                                                                                                 \
    static void INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, VarName)()                                \
    {                                                                                                            \
        static auto once = INTL_PERFKIT_NS_1::register_conf_function(&_internal_self_t::VarName);                \
    }                                                                                                            \
                                                                                                                 \
    static INTL_PERFKIT_NS_0::config_attribute_ptr INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName)() \
    {                                                                                                            \
        static auto instance                                                                                     \
                = INTL_PERFKIT_NS_0::config_attribute_factory<decltype(VarName)::value_type>{#VarName, UserKey}  \
                          ._internal_default_value(DefaultValue)                                                 \
                                  __VA_ARGS__                                                                    \
                          .confirm();                                                                            \
                                                                                                                 \
        return instance;                                                                                         \
    }

#define PERFKIT_CFG_ITEM_LAZY2(VarName, DefaultValue, ...) PERFKIT_CFG_ITEM_LAZY(VarName, DefaultValue, {}, __VA_ARGS__)

/**
 * This defines attribute variable as static inline, which causes overhead on program startup.
 *
 * NOTE: On porting from legacy perfkit, default value must be
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
 * Globally accessible repository. Place this somwhere of your program exactly once!
 */
#define PERFKIT_CFG_GLOBAL_REPO_define_body(Namespace)                                      \
    namespace Namespace {                                                                   \
    INTL_PERFKIT_NS_0::config_registry_ptr() registry()                                   \
    {                                                                                       \
        static auto _rg = INTL_PERFKIT_NS_0::config_registry::_internal_create(#Namespace); \
        return _rg;                                                                         \
    }                                                                                       \
    }

/**
 * Forward
 */
#define PERFKIT_CFG_GLOBAL_REPO_namespace(Namespace)       \
    namespace Namespace {                                  \
    INTL_PERFKIT_NS_0::config_registry_ptr() registry(); \
    }                                                      \
    namespace Namespace

/**
 * Create globally accessible
 */
#define PERFKIT_CFG_GLOBAL_SET(ClassName, InstanceName, ...) \
    ClassName InstanceName = ClassName::_bk_create_global(registry(), #InstanceName, ##__VA_ARGS__);
