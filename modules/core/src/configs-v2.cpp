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

#include <set>

#include <fmt/core.h>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "cpph/refl/archive/json.hpp"
#include "cpph/refl/archive/msgpack-writer.hxx"
#include "cpph/utility/singleton.hxx"
#include "perfkit/configs-v2.h"
#include "perfkit/detail/base.hpp"
#include "perfkit/detail/configs-v2-backend.hpp"

namespace vw = ranges::views;

static auto CPPH_LOGGER() { return perfkit::glog().get(); }

//*
#define print fmt::print("[{}:{}] ({}): ", __FILE__, __LINE__, __func__), fmt::print
/*/
#define primt(...)
//*/

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
    _self->_events.post(
            [this,
             arg = move(arg),
             order = ++_self->_sort_id_gen,
             prefix = move(prefix)]() mutable {
                auto& [sort_order, dst_prefix] = _self->_config_added[arg];
                sort_order = order;
                dst_prefix = move(prefix);
            });
}

void config_registry::_internal_item_remove(config_base_wptr arg)
{
    _self->_events.post([this, arg = move(arg)]() mutable {
        // Abort pending insertion
        _self->_config_added.erase(arg);

        // Keep erased list sorted.
        auto iter = lower_bound(_self->_config_removed, arg, std::owner_less<>{});
        if (iter == _self->_config_removed.end() || not ptr_equals(*iter, arg))
            _self->_config_removed.insert(iter, arg);
    });
}

void config_registry::item_notify()
{
    _self->_events.post([this] { _self->_flag_add_remove_notified = true; });
}

bool config_registry::update()
{
    // Perform update from backend reference
    _self->_do_update();

    // update() includes call to check_update(), which manages local cache for modification fence.
    return check_update();
}

size_t config_registry::fence() const
{
    return acquire(_self->_fence);
}

config_registry_id_t config_registry::id() const noexcept
{
    return _self->_id;
}

bool config_registry::_internal_commit_value_user(
        config_base_ptr ref, refl::shared_object_ptr view)
{
    return _self->_commit(move(ref), move(view));
}

bool config_registry::is_registered() const
{
    return acquire(_self->_is_registered);
}

bool config_registry::backend_t::_commit(config_base_ptr ref, refl::shared_object_ptr candidate)
{
    auto& attrib = ref->attribute();
    if (
            (not attrib->fn_minmax_validate || attrib->fn_minmax_validate(candidate.view()))  //
            &&                                                                                //
            (not attrib->fn_validate || attrib->fn_validate(candidate.view()))                //
    ) {
        CPPH_TMPVAR{lock_guard{_mtx_access}};
        auto ctx = find_ptr(_configs, ref->id());
        if (not ctx) { return false; }  // Possibly removed during validation

        swap(ctx->second._staged, candidate);
        set_push(_refreshed_items, ref->id());

        return true;
    } else {
        return false;
    }
}

bool config_registry::backend_t::bk_commit(config_base_ptr ref, archive::if_reader* content)
{
    auto object = ref->_context.attribute->fn_construct();

    try {
        *content >> object.view();
    } catch (std::exception& e) {
        return false;
    }

    return _commit(move(ref), move(object));
}

void config_registry::backend_t::_do_update()
{
    // If it's first call after creation, register this to global repository.
    std::call_once(_register_once_flag, &backend_t::_register_to_global_repo, this);

    // Event entities ...
    bool has_structure_change = false;
    list<config_base_ptr> updates;

    auto pool = &_free_evt_nodes;
    auto checkout_push =
            [pool](list<config_base_ptr>* to, decltype(updates)::value_type ptr) {
                auto iter = pool->empty() ? pool->emplace(pool->end()) : pool->begin();
                *iter = forward<decltype(ptr)>(ptr);
                to->splice(to->end(), *pool, iter);
            };

    // Perform update inside protected scope
    {
        CPPH_TMPVAR = lock_guard{_mtx_access};

        _events.flush();

        // Check for insertion / deletions
        if (exchange(_flag_add_remove_notified, false)) {
            // Check for deletions
            for (auto& wp : _config_removed) {
                if (auto conf = wp.lock()) {
                    _configs.erase(conf->_id);  // Erase from 'all' list
                    has_structure_change = true;
                }
            }

            // Check for additions
            for (auto& [wp, tup] : _config_added) {
                if (auto conf = wp.lock()) {
                    auto& [sort_order, prefix] = tup;
                    if (not conf) { continue; }

                    auto [iter, is_new] = _configs.try_emplace(conf->id());
                    assert(is_new || ptr_equals(iter->second.reference, conf) && "Reference must not change!");
                    has_structure_change |= is_new;

                    iter->second.reference = conf;
                    iter->second.sort_order = sort_order;
                    iter->second.full_key = move(prefix.append(conf->name()));
                }
            }

            _config_added.clear();
            _config_removed.clear();
        }

        // Check for updates
        if (not _refreshed_items.empty()) {
            for (auto id : _refreshed_items) {
                // Check if config is erased
                auto iter = _configs.find(id);
                if (iter == _configs.end()) { continue; }

                // Check if config is expired
                auto conf = iter->second.reference.lock();
                if (not conf) {
                    // Configuration is expired ... collect garbage.
                    has_structure_change = true;
                    _configs.erase(iter);
                    continue;
                }

                // Perform actual update
                assert(iter->second._staged && "Staged data must be prepared!");
                conf->attribute()->fn_swap_value(conf->_context.raw_data, iter->second._staged);
                iter->second._staged.reset();  // Clear staged data

                // Append to 'updated' list.
                checkout_push(&updates, move(conf));
            }

            _refreshed_items.clear();
        }

        // If there is any update, increase fence once.
        if (has_structure_change || not updates.empty()) {
            _fence.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Publish updates to subscribers
    if (has_structure_change) { evt_structure_changed.invoke(_owner); }
    if (not updates.empty()) { evt_updated_entities.invoke(_owner, updates); }
}

static auto& global_config_storage() noexcept
{
    return default_singleton<map<string, map<string, string>>>();
}

static auto _global_repo() noexcept
{
    static std::mutex _lock;
    static std::map<config_registry_id_t, weak_ptr<config_registry>> _repo;

    return make_pair(std::unique_lock{_lock}, &_repo);
}

void config_registry::backend_t::_register_to_global_repo()
{
    CPPH_DEBUG("Registering '{}' to global repository ...", _name);

    // Load values from the global config rawdata storage
    {
            // TODO
    }

    // Register to global repository
    {
        auto [lock, repo] = _global_repo();
        auto is_new = repo->try_emplace(_id, _owner->weak_from_this()).second;
        assert(is_new && "Config registry can't be registered twice.");

        release(_is_registered, true);
    }

    g_evt_registered.invoke(_owner->shared_from_this());
}

void config_registry::backend_t::all_items(vector<config_base_ptr>* out) const noexcept
{
    // Retrieve all alive elements
    {
        CPPH_TMPVAR{std::shared_lock{_mtx_access}};
        out->reserve(out->size() + _configs.size());

        copy(_configs | vw::transform([](auto&& p) { return p.second.reference.lock(); }) | vw::filter([](auto&& p) { return bool(p); }),
             back_inserter(*out));
    }
}

void config_registry::backend_t::enumerate_registries(vector<shared_ptr<config_registry>>* out) noexcept
{
    // Retrieve all alive elements
    {
        auto [lock, repo] = _global_repo();
        out->reserve(out->size() + repo->size());

        copy(*repo | vw::transform([](auto&& p) { return p.second.lock(); }) | vw::filter([](auto&& p) { return bool(p); }),
             back_inserter(*out));
    }
}

bool config_registry::unregister()
{
    if (not _self->_is_registered.exchange(false)) {
        // Instance already unregistered. Do nothing.
        return false;
    }

    CPPH_DEBUG("Unregistering '{}' from global registry ...");
    {
        auto [lock, repo] = _global_repo();
        auto erase_result = repo->erase(id());
        assert(erase_result == 1 && "Once registered, registry must be unregistered exactly once.");
    }

    // Propagate un-registration
    backend_t::g_evt_unregistered.invoke(this);

    return true;
}

void configs_dump_all(string* json_dst)
{
    // TODO
}

void configs_export_to(string_view path)
{
    // TODO
}

bool configs_import_content(string_view json_content)
{
    // TODO
    return false;
}

bool configs_import_file(string_view path)
{
    // TODO
    return false;
}

}  // namespace perfkit::v2

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
