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
#include <spdlog/spdlog.h>

#include "cpph/refl/archive/json.hpp"
#include "cpph/refl/archive/msgpack-reader.hxx"
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
        config_base* ref, refl::shared_object_ptr view)
{
    return _self->_commit(ref, move(view));
}

bool config_registry::is_registered() const
{
    return acquire(_self->_is_registered);
}

bool config_registry::backend_t::_commit(config_base* ref, refl::shared_object_ptr candidate)
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

bool config_registry::backend_t::bk_commit(config_base* ref, archive::if_reader* content)
{
    auto object = ref->_body.attribute->fn_construct();

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
        CPPH_TMPVAR{lock_guard{_mtx_access}};

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

            // Rebuild config id mappings
            _key_id_table.clear();
            for (auto& [key, ctx] : _configs) {
                _key_id_table.try_emplace(ctx.full_key, key);
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
                auto* node = &iter->second;
                auto conf = node->reference.lock();
                if (not conf) {
                    // Configuration is expired ... collect garbage.
                    has_structure_change = true;
                    _configs.erase(iter);
                    continue;
                }

                // Perform actual update
                assert(node->_staged && "Staged data must be prepared!");
                conf->attribute()->fn_swap_value(conf->_body.raw_data, node->_staged);
                node->_staged.reset();  // Clear staged data

                // Increase modification fence
                conf->_fence_modified.fetch_add(1);

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

static auto _global_repo() noexcept
{
    static std::mutex _lock;
    static std::map<config_registry_id_t, weak_ptr<config_registry>> _repo;

    return make_pair(std::unique_lock{_lock}, &_repo);
}

void config_registry::backend_t::all_items(vector<config_base_ptr>* out) const noexcept
{
    // Retrieve all alive elements
    {
        CPPH_TMPVAR{std::shared_lock{_mtx_access}};
        out->reserve(out->size() + _configs.size());

        for (auto& [_, ctx] : _configs)
            if (auto cfg = ctx.reference.lock())
                out->emplace_back(move(cfg));
    }
}

void config_registry::backend_t::enumerate_registries(vector<config_registry_ptr>* out) noexcept
{
    // Retrieve all alive elements
    {
        auto [lock, repos] = _global_repo();
        out->reserve(out->size() + repos->size());

        for (auto& [_, wrepo] : *repos)
            if (auto repo = wrepo.lock(); repo && repo->is_registered())
                out->emplace_back(move(repo));
    }
}

bool config_registry::unregister()
{
    if (not _self->_is_registered.exchange(false)) {
        // Instance already unregistered. Do nothing.
        return false;
    }

    CPPH_DEBUG("Unregistering '{}' from global registry ...", name());
    {
        auto [lock, repo] = _global_repo();
        auto erase_result = repo->erase(id());
        assert(erase_result == 1 && "Once registered, registry must be unregistered exactly once.");
    }

    // Propagate un-registration
    backend_t::g_evt_unregistered.invoke(this);
    return true;
}

void config_registry::set_transient()
{
    release(_self->_is_transient, true);
}

void config_registry::unset_transient()
{
    release(_self->_is_transient, false);
}

bool config_registry::is_transient() const
{
    return acquire(_self->_is_transient);
}
}  // namespace perfkit::v2

/*
 *
 * GLOBAL CONFIGURATIONS
 *
 */
#include "cpph/refl/types/nlohmann_json.hxx"
#include "cpph/refl/types/tuple.hxx"

namespace perfkit::v2 {
static auto _g_config() noexcept
{
    static std::mutex _lock;
    static global_config_storage_t _storage;

    return pair{std::unique_lock{_lock}, &_storage};
}

void config_registry::backend_t::_register_to_global_repo()
{
    CPPH_DEBUG("Registering '{}' to global repository ...", _name);

    if (not acquire(_is_transient)) {
        auto [_, p_conf] = _g_config();

        if (auto pair = find_ptr(*p_conf, _name)) {
            // Load values from the global config rawdata storage
            _owner->import_from(pair->second);
        } else {
            // If registry or any element is missing from global storage, merge on it.
            _owner->export_to(&(*p_conf)[_name]);
        }
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

bool config_registry::import_from(config_registry_storage_t const& from, string* buf)
{
    auto& s = *_self;
    map<string, config_base_ptr> key_conf_table;

    // Clone key to id table
    {
        CPPH_TMPVAR{std::shared_lock{s._mtx_access}};
        for (auto& [key, id] : s._key_id_table)
            if (auto elem = find_ptr(s._configs, id))
                if (auto ref = elem->second.reference.lock())
                    key_conf_table[key] = ref;
    }

    string buf_body;
    if (buf == nullptr) { buf = &buf_body; }

    // Prepare context
    buf->clear();
    streambuf::stringbuf sbuf{buf};
    archive::msgpack::writer writer{&sbuf};
    archive::msgpack::reader reader{&sbuf};

    for (auto& [key, json] : from) {
        auto p_conf = find_ptr(key_conf_table, key);
        if (not p_conf) { continue; }  // Missing element

        // Clear context
        sbuf.clear(), writer.clear(), reader.clear();

        // Serialize JSON data
        writer << json;
        writer.flush();

        // Retrieve from dumped json content, and commit
        s.bk_commit(p_conf->second.get(), &reader);
    }

    return false;
}

void config_registry::export_to(config_registry_storage_t* to, string* buf) const
{
    auto& s = *_self;

    string buf_body;
    if (buf == nullptr) { buf = &buf_body; }

    // Export must be performed inside 'access protected' scope.
    CPPH_TMPVAR{std::shared_lock{s._mtx_access}};

    // Filled with msgpack raw binary
    buf->clear();
    streambuf::stringbuf sbuf{buf};
    archive::msgpack::writer writer{&sbuf};
    archive::msgpack::reader reader{&sbuf};

    for (auto& [key, ctx] : s._configs) {
        auto cfg = ctx.reference.lock();
        if (not cfg || not cfg->attribute()->can_export) { continue; }

        // Clear context
        sbuf.clear(), writer.clear(), reader.clear();

        // cfg -> msgpack -> nlohmann::json
        // If it has staged value, prefer it here.
        if (ctx._staged) {
            writer << ctx._staged.view();
        } else {
            writer << cfg->_body.raw_data.view();
        }

        writer.flush();
        reader >> (*to)[ctx.full_key];
    }
}

void configs_export(global_config_storage_t* json_dst)
{
    //  Iterate registries, merge to existing global, copy to json_dst.
    vector<config_registry_ptr> repos;
    config_registry::backend_t::enumerate_registries(&repos);

    // Copy existing global configs to destination
    {
        auto [_, p_all] = _g_config();
        string buffer_;

        // Collect all alive registries' configs
        for (auto& repo : repos) {
            repo->export_to(&(*p_all)[repo->name()], &buffer_);
        }

        // Update existing global configs with new entries
        *json_dst = *p_all;
    }
}

void configs_import(global_config_storage_t json_content)
{
    // Swap content with existing globals
    {
        auto [_, p_all] = _g_config();
        *p_all = move(json_content);

        string buffer_;
        vector<config_registry_ptr> repos;
        config_registry::backend_t::enumerate_registries(&repos);

        // Sort repositories by name.
        sort(repos, [](auto&& a, auto&& b) { return a->name() < b->name(); });

        // Import configs
        for (auto& [key, content] : *p_all) {
            auto iter = lower_bound(repos, key, [](auto&& a, auto&& b) { return a->name() < b; });
            if (iter == repos.end() || (**iter).name() != key) { continue; }  // could not found.

            (**iter).import_from(content, &buffer_);
        }
    }
}
}  // namespace perfkit::v2

#include <fstream>

namespace perfkit::v2 {
bool configs_export(string_view path)
{
    global_config_storage_t all;
    configs_export(&all);

    std::ofstream fs{string{path}};
    if (not fs.is_open()) { return false; }

    archive::json::writer{fs.rdbuf()} << all;
    CPPH_INFO("Config imported from '{}'", path);
    return true;
}

bool configs_import(string_view path)
{
    global_config_storage_t all;

    std::ifstream fs{string{path}};
    if (not fs.is_open()) { return false; }

    try {
        archive::json::reader{fs.rdbuf()} >> all;
        configs_import(move(all));
    } catch (std::exception& e) {
        CPPH_ERROR("Parse error from config {}");
        return false;
    }

    return true;
}
}  // namespace perfkit::v2

/*
 *
 * PERFKIT_CFG_CLASS MACRO DEFINITION
 * * Reference code for macro definition
 *
 */
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
