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

#include "perfkit/detail/configs-v2.hpp"

#include "perfkit/configs-v2.h"
#include "perfkit/detail/configs-v2-backend.hpp"

namespace perfkit::v2 {
namespace _configs {
void parse_full_key(
        const string& full_key, string* o_display_key, vector<string_view>* o_hierarchy)
{
    // TODO: Parse full_key to display key
}

void verify_flag_string(string_view str)
{
    // TODO: if any character other than -_[a-z][A-Z][0-9], and --no- prefixed, and 'h' is not allowed
}
}  // namespace _configs

config_registry::config_registry(ctor_constraint_t, std::string name)
        : _self(make_unique<backend_t>(this, move(name)))
{
}

config_registry::~config_registry() noexcept
{
    unregister();
}

string const& config_registry::name() const
{
    return _self->_name;
}

void config_registry::_internal_value_read_lock() { _self->_mtx_access.lock_shared(); }
void config_registry::_internal_value_read_unlock() { _self->_mtx_access.unlock_shared(); }
void config_registry::_internal_value_write_lock() { _self->_mtx_access.lock(); }
void config_registry::_internal_value_write_unlock() { _self->_mtx_access.unlock(); }

void config_registry::_internal_item_add(config_base_wptr arg, string prefix)
{
    _self->_events.post([arg = move(arg), prefix = move(prefix)] {

    });
}

void config_registry::_internal_item_remove(config_base_wptr arg)
{
    // TODO
}

bool config_registry::update()
{
    // If it's first call after creation, register this to global repository.
    std::call_once(_self->_register_once_flag, &backend_t::_register_to_global_repo, _self.get());

    //

    // Perform update inside protected scope
    if (CPPH_TMPVAR = lock_guard{_self->_mtx_update}; true) {
        _self->_events.flush();

        // Check for refreshed entities ...
    }

    return false;
}

size_t config_registry::fence() const
{
    return acquire(_self->_fence);
}

}  // namespace perfkit::v2

//
//
//
//
//
#if 1
#endif

//
// PERFKIT_CFG_CLASS MACRO DEFINITION
// * Reference code for macro definition
//
//
#if 0
#    include <cpph/refl/object.hxx>

#    define DefaultValue 3
#    define InstanceName MySubSet
#    define MY_VA_ARGS_  3, "hello"

class MyConf : public INTL_PERFKIT_NS_0::config_set<MyConf>
{
    INTL_PERFKIT_NS_0::config<INTL_PERFKIT_NS_1::deduced_t<decltype(DefaultValue)>> VarName
            = {(INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, VarName)(),
                INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName)())};

    static void INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, VarName)()
    {
        static auto once = INTL_PERFKIT_NS_1::register_conf_function(&_internal_self_t::VarName);
    }

    static INTL_PERFKIT_NS_0::config_attribute_ptr INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName)()
    {
        static auto instance
                = INTL_PERFKIT_NS_0::config_attribute_factory<decltype(VarName)::value_type>{"VarName", "UserKey"}
                          ._internal_default_value(DefaultValue)
                          .edit_mode(perfkit::v2::edit_mode::path)
                          .clamp(-4, 11)
                          .validate([](int& g) { return g > 0; })
                          .verify([](int const& r) { return r != 0; })
                          .one_of({1, 2, 3})
                          .description("Hello!")
                          .flags("g", 'g', "GetMyFlag")
                          .confirm();

        return instance;
    }

    class ClassName : public config_set<ClassName>
    {
        // Legacy ...
        INTL_PERFKIT_NS_0::config<INTL_PERFKIT_NS_1::deduced_t<decltype(INTL_PERFKIT_NS_1::default_from_va_arg(MY_VA_ARGS_))>> VarName
                = {(INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, VarName)(),
                    INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName))};

        static void INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, VarName)()
        {
            static auto once = INTL_PERFKIT_NS_1::register_conf_function(&_internal_self_t::VarName);
        }

        static inline auto const INTERNAL_CPPH_CONCAT(_internal_perfkit_attribute_, VarName)
                = INTL_PERFKIT_NS_0::config_attribute_factory<decltype(VarName)::value_type>{"VarName", INTL_PERFKIT_NS_1::name_from_va_arg(MY_VA_ARGS_)}
                          ._internal_default_value(INTL_PERFKIT_NS_1::default_from_va_arg(MY_VA_ARGS_))
                          .edit_mode(perfkit::v2::edit_mode::path)
                          .clamp(-4, 11)
                          .validate([](int& g) { return g > 0; })
                          .verify([](int const& r) { return r != 0; })
                          .one_of({1, 2, 3})
                          .description("Hello!")
                          .flags("g", 'g', "GetMyFlag")
                          .confirm();
    };

    // #define PERFKIT_CFG_SUBSET(SetType, VarName, ...) ...(this, #VarName, ##__VA_ARGS__) ...
    ClassName InstanceName = ClassName::_bk_make_subobj(
            (INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, InstanceName)(), this),
            "#InstanceName", "##__VA_ARGS__");

    static void INTERNAL_CPPH_CONCAT(_internal_perfkit_register_conf_, InstanceName)()
    {
        static auto once = INTL_PERFKIT_NS_1::register_subset_function(&_internal_self_t::InstanceName);
    }
};

void foof()
{
    auto r = MyConf::create("hell");
}

#endif
