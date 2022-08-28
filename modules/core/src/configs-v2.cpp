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

#include <regex>
#include <set>

#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "cpph/refl/archive/json.hpp"
#include "cpph/refl/archive/msgpack-reader.hxx"
#include "cpph/refl/archive/msgpack-writer.hxx"
#include "cpph/utility/singleton.hxx"
#include "perfkit/configs-v2.h"
#include "perfkit/detail/base.hpp"
#include "perfkit/detail/configs-v2-backend.hpp"

static auto CPPH_LOGGER() { return perfkit::glog().get(); }

//*
#define print fmt::print("[{}:{}] ({}): ", __FILE__, __LINE__, __func__), fmt::print
/*/
#define primt(...)
//*/

namespace perfkit::v2 {
config_base::~config_base() noexcept
{
    if (_rg) {
        _rg->backend()->_internal_notify_config_disposal();
    }
}

namespace _configs {

void verify_flag_string(string_view str)
{
    assert(not str.empty() && "Empty string not allowed!");
    assert(isalnum(str[0]) && "First character must be alphanumeric!");
    assert(str != "h" && str != "help" && "'HELP' flag is reserved!");
    assert(str.find("no-") != 0 && "'no-' prefixed flag is reserved!");
    assert(str.find("--") == string::npos && "Consecutive '--' is not allowed!");
    assert(all_of(str, [](auto c) { return isalnum(c) || c == '-' || c == '_'; }));
}
}  // namespace _configs

config_registry::config_registry(ctor_constraint_t, std::string name, bool is_global)
        : _self(make_unique<backend_t>(this, move(name), is_global)),
          on_update(&_self->evt_update_listener)
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

void config_registry::_internal_item_add(config_base_ptr arg, string prefix)
{
    {
        CPPH_TMPVAR = lock_guard{arg->_mtx_full_key};

        assert(not arg->_rg);
        arg->_rg = shared_from_this();

        auto prefix_len = prefix.size();
        arg->_cached_full_key = move(prefix) + arg->name();
        arg->_cached_prefix = string_view{arg->_cached_full_key}.substr(0, prefix_len);
    }

    _self->_events.post(
            [this,
             arg = move(arg),
             order = ++_self->_sort_id_gen] {
                auto& [sort_order] = _self->_config_added[arg];
                sort_order = order;
            });
}

void config_registry::_internal_item_remove(config_base_wptr arg)
{
    if (auto rg = arg.lock()) {
        CPPH_TMPVAR = lock_guard{rg->_mtx_full_key};

        assert(ptr_equals(rg->_rg, weak_from_this()));
        rg->_rg.reset();
    }

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

static bool attribute_validate(config_attribute const& attr, refl::object_view_t view)
{
    return (not attr.fn_minmax_validate || attr.fn_minmax_validate(view))
        && (not attr.fn_validate || attr.fn_validate(view));
}

bool config_registry::_internal_commit_inplace_user(config_base* ref, refl::object_view_t view)
{
    return backend()->_internal_commit_inplace(ref, view);
}

bool config_registry::is_registered() const
{
    return acquire(_self->_is_registered);
}

bool config_registry::backend_t::_internal_commit_inplace(config_base* ref, refl::object_view_t view)
{
    if (attribute_validate(*ref->attribute(), view)) {
        {
            CPPH_TMPVAR{lock_guard{ref->_mtx_raw_access}};
            ref->attribute()->fn_swap_value(ref->_body.raw_data.view(), view);
        }

        CPPH_TMPVAR{lock_guard{_mtx_access}};
        auto ctx = find_ptr(_configs, ref->id());
        if (not ctx) { return false; }  // Possibly removed during validation

        set_push(_inplace_updates, ref->id());
        release(_has_update, true);

        return true;
    }

    return false;
}

bool config_registry::backend_t::_commit(config_base* ref, refl::shared_object_ptr candidate)
{
    if (attribute_validate(*ref->attribute(), candidate.view())) {
        CPPH_TMPVAR{lock_guard{_mtx_access}};
        auto ctx = find_ptr(_configs, ref->id());
        if (not ctx) { return false; }  // Possibly removed during validation

        swap(ctx->second._staged, candidate);
        set_push(_refreshed_items, ref->id());
        release(_has_update, true);

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

    return _commit(ref, move(object));
}

void config_registry::backend_t::_do_update()
{
    // Event entities ...
    bool has_structure_change = false;
    bool has_committed_update = false;
    list<config_base_ptr> updates;

    auto pool = &_free_evt_nodes;
    auto checkout_push =
            [pool](list<config_base_ptr>* to, decltype(updates)::value_type ptr) {
                auto iter = pool->empty() ? pool->emplace(pool->end()) : pool->begin();
                *iter = forward<decltype(ptr)>(ptr);
                to->splice(to->end(), *pool, iter);
            };

    if (not _events.empty()) {
        CPPH_TMPVAR{lock_guard{_mtx_access}};

        // Flush pending events
        _events.flush();

        // Check if there's any structure change ...
        has_structure_change = exchange(_flag_add_remove_notified, false);
    }

    // Perform register/unregister
    if (exchange(has_structure_change, false)) {
        has_structure_change = _handle_structure_update();
    }

    // If it's first call after creation, register this to global repository.
    std::call_once(_register_once_flag, &backend_t::_register_to_global_repo, this);

    // Perform update inside protected scope
    if (bool has_update; try_exchange(_has_update, false, &has_update) && has_update) {
        CPPH_TMPVAR{lock_guard{_mtx_access}};

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
                    release(_has_expired_ref, true);
                    continue;
                }

                // Perform actual update
                assert(node->_staged && "Staged data must be prepared!");

                {
                    CPPH_TMPVAR{lock_guard{conf->_mtx_raw_access}};
                    conf->attribute()->fn_swap_value(conf->_body.raw_data.view(), node->_staged.view());
                }

                node->_staged.reset();  // Clear staged data

                // Increase modification fence
                conf->_fence_modified.fetch_add(1);

                // Append to 'updated' list.
                checkout_push(&updates, move(conf));
                has_committed_update = true;
            }

            _refreshed_items.clear();
        }

        if (not _inplace_updates.empty()) {
            for (auto id : _inplace_updates) {
                // Check if config is erased
                auto iter = _configs.find(id);
                if (iter == _configs.end()) { continue; }

                // Check if config is expired
                auto* node = &iter->second;
                auto conf = node->reference.lock();
                if (not conf) {
                    // Configuration is expired ... collect garbage.
                    release(_has_expired_ref, true);
                    continue;
                }

                // Append to 'updated' list.
                checkout_push(&updates, move(conf));
            }

            _inplace_updates.clear();
        }
    }

    if (bool has_disposal; try_exchange(_has_expired_ref, false, &has_disposal)) {
        auto n_expired = erase_if_each(
                _configs, [](auto&& pair) { return pair.second.reference.expired(); });

        has_structure_change |= (n_expired != 0);
        CPPH_DEBUG("Config Repo '{}': {} configs disposed", _name, n_expired);
    }

    // If there is any update, increase fence once.
    if (has_structure_change || has_committed_update) {
        _fence.fetch_add(1, std::memory_order_relaxed);
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

void config_registry::backend_t::bk_all_items(vector<config_base_ptr>* out) const noexcept
{
    // Retrieve all alive elements
    CPPH_TMPVAR{std::shared_lock{_mtx_access}};
    out->reserve(out->size() + _configs.size());

    // Retrieve them as sorted
    auto all_ = (config_entity_context const**)alloca(_configs.size() * sizeof(void*));
    array_view<config_entity_context const*> all{all_, _configs.size()};
    transform(_configs, all.begin(), [](auto& p) { return &p.second; });

    sort(all, [](auto a, auto b) { return a->sort_order < b->sort_order; });

    for (auto& ctx : all)
        if (auto cfg = ctx->reference.lock())
            out->emplace_back(move(cfg));
}

void config_registry::backend_t::bk_enumerate_registries(vector<config_registry_ptr>* out, bool include_unregistered) noexcept
{
    // Retrieve all alive elements
    auto [lock, repos] = _global_repo();
    out->reserve(out->size() + repos->size());

    for (auto& [_, wrepo] : *repos)
        if (auto repo = wrepo.lock(); repo && (include_unregistered || repo->is_registered()))
            out->emplace_back(move(repo));
}

bool config_registry::unregister()
{
    if (_self->_is_global) {
        // Ignore unregister request for global repos
        return false;
    }

    if (not _self->_is_registered.exchange(false)) {
        // Instance already unregistered. Do nothing.
        return false;
    }

    CPPH_DEBUG("Unregistering '{}' from global registry ...", name());
    assert(not _self->_is_transient);
    {
        auto [lock, repo] = _global_repo();
        auto erase_result = repo->erase(id());
        assert(erase_result == 1 && "Once registered, registry must be unregistered exactly once.");
    }

    // Propagate un-registration
    backend_t::g_evt_unregistered.invoke(weak_from_this());
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
#include "cpph/refl/types/nlohmann_json_fwd.hpp"
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

    if (acquire(_is_transient))
        return;

    // If registry or any element is missing from global storage, merge on it.
    if (auto [_, p_conf] = _g_config(); not find_ptr(*p_conf, _name))
        _owner->export_to(&(*p_conf)[_name]);

    // Register to global repository
    for (;;) {
        auto [lock, repo] = _global_repo();
        auto is_new = repo->try_emplace(_id, _owner->weak_from_this()).second;

        if (not is_new) {
            CPPH_WARN(
                    "Waiting for registry '{}' unregistered from global registry.\n"
                    "This might cause deadlock if you don't unregister another\n"
                    " repository that has same name.");
            std::this_thread::sleep_for(1s);
            continue;
        }

        release(_is_registered, true);
        break;
    }

    g_evt_registered.invoke(_owner->shared_from_this());
}

bool config_registry::backend_t::_handle_structure_update()
{
    decltype(_config_added) config_added;
    bool has_structure_change = false;
    {
        CPPH_TMPVAR{lock_guard{_mtx_access}};
        config_added = move(_config_added);

        // Check for deletions
        for (auto& wp : _config_removed) {
            if (auto conf = wp.lock()) {
                _configs.erase(conf->_id);  // Erase from 'all' list
                has_structure_change = true;
            }
        }

        // Check for additions
        for (auto& [wp, tup] : config_added) {
            if (auto conf = wp.lock()) {
                auto& [sort_order] = tup;
                if (not conf) { continue; }

                auto [iter, is_new] = _configs.try_emplace(conf->id());
                assert(is_new || ptr_equals(iter->second.reference, conf) && "Reference must not change!");
                has_structure_change |= is_new;

                auto& [key, ctx] = *iter;

                ctx.reference = conf;
                ctx.sort_order = sort_order;
                ctx.full_key_cached = conf->full_key();
            }
        }

        // Rebuild config id mappings
        _key_id_table.clear();
        for (auto& [key, ctx] : _configs) {
            _key_id_table.try_emplace(ctx.full_key_cached, key);
        }

        _config_removed.clear();
    }

    // Performs initial loading on adding new storage ...
    optional<config_registry_storage_t> storage;
    struct rw_context_t {
        string str;
        streambuf::stringbuf sbuf{&str};
        archive::msgpack::writer wr{&sbuf};
        archive::msgpack::reader rd{&sbuf};
    };
    optional<rw_context_t> rw;

    for (auto& [wp, tup] : config_added) {
        auto conf = wp.lock();
        if (not conf) { continue; }

        if (not storage) {
            if (auto p_storage = find_ptr(*_g_config().second, _name))
                storage = p_storage->second;
            else
                storage.emplace();  // There's no storage for this config registry ...
        }

        if (not rw) { rw.emplace(); }
        rw->sbuf.clear();
        rw->wr.clear(), rw->rd.clear();

        if (auto data = find_ptr(*storage, conf->full_key())) {
            // Load existing value ...
            (rw->wr << data->second).flush();
            bk_commit(conf.get(), &rw->rd);
        } else {
            // Init with default value ...
            (rw->wr << conf->_body.raw_data.view()).flush();
            try {
                rw->rd >> (*storage)[conf->full_key()];
            } catch (std::exception& e) {
                CPPH_ERROR("Error dumping json object");
            }
        }
    }

    if (storage) {
        auto [lc, p_cfg] = _g_config();
        (*p_cfg)[_name] = move(*storage);
    }

    return has_structure_change;
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
        if (not p_conf || not p_conf->second->can_import()) { continue; }  // Missing element

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
        if (not cfg || not cfg->can_export()) { continue; }

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
        reader >> (*to)[ctx.full_key_cached];
    }
}

void configs_export(global_config_storage_t* json_dst, bool merge)
{
    //  Iterate registries, merge to existing global, copy to json_dst.
    vector<config_registry_ptr> repos;
    config_registry::backend_t::bk_enumerate_registries(&repos);

    json_dst->clear();

    if (merge) {
        auto [_, p_all] = _g_config();
        *json_dst = *p_all;
    }

    // Copy existing global configs to destination
    string buffer_;
    {
        // Collect all alive registries' configs
        for (auto& repo : repos) {
            repo->export_to(&(*json_dst)[repo->name()], &buffer_);
        }
    }

    // Update existing global configs with new entries
    {
        auto [_, p_all] = _g_config();
        *p_all = *json_dst;
    }
}

void configs_import(global_config_storage_t json_content)
{
    // Import json content
    {
        vector<config_registry_ptr> repos;
        config_registry::backend_t::bk_enumerate_registries(&repos);

        // Sort repositories by name.
        sort(repos, [](auto&& a, auto&& b) { return a->name() < b->name(); });

        // Import configs
        string buffer_;
        for (auto const& [key, content] : json_content) {
            auto iter = lower_bound(repos, key, [](auto&& a, auto&& b) { return a->name() < b; });
            if (iter == repos.end() || (**iter).name() != key) { continue; }  // could not found.

            (**iter).import_from(content, &buffer_);
            (**iter).backend()->bk_notify();  // Notify update
        }
    }

    // Copy content with existing globals
    {
        auto [_, p_all] = _g_config();
        *p_all = move(json_content);
    }
}
}  // namespace perfkit::v2

#include <fstream>

namespace perfkit::v2 {
bool configs_export(string_view path, bool merge)
{
    global_config_storage_t all;
    configs_export(&all, merge);

    std::ofstream fs{string{path}};
    if (not fs.is_open()) { return false; }

    {
        archive::json::writer writer{fs.rdbuf()};
        writer.indent = 2;
        writer << all;
    }

    CPPH_INFO("Config exported to '{}'", path);
    return true;
}

bool configs_import(string_view path)
{
    global_config_storage_t all;

    std::ifstream fs{string{path}};
    if (not fs.is_open()) { return false; }

    try {
        archive::json::reader{fs.rdbuf()} >> all;
    } catch (std::exception& e) {
        CPPH_ERROR("Parse error from config {}", path);
        return false;
    }

    configs_import(move(all));
    CPPH_INFO("Config successfully imported from '{}'", path);
    return true;
}

namespace _configs {
void parse_full_key(
        const string& full_key, string* o_display_key, vector<string_view>* o_hierarchy)
{
    *o_display_key = full_key;
    o_hierarchy->clear();

    string_view active_view = full_key;
    for (;;) {
        auto pos = active_view.find_first_of('|');
        if (pos == string_view::npos) {
            o_hierarchy->push_back(active_view);
            break;
        }
        assert(pos != 0);

        auto tok = active_view.substr(0, pos);
        active_view = active_view.substr(pos + 1);

        o_hierarchy->push_back(tok);
    }

    //    // TODO: Parse full_key to display key
    //    static std::regex rg_trim_whitespace{R"((?:^|\|?)\s*(\S?[^|]*\S)\s*(?:\||$))"};
    //    static std::regex rg_remove_order_marker{R"(\+[^|]+\|)"};
    //
    //    o_display_key->clear();
    //    o_hierarchy->clear();
    //
    //    o_display_key->reserve(full_key.size());
    //    for (std::cregex_iterator end{},
    //         iter{full_key.c_str(), full_key.c_str() + full_key.size(), rg_trim_whitespace};
    //         iter != end;
    //         ++iter) {
    //        if (not iter->ready()) { throw "Invalid Token"; }
    //        auto token = string_view{full_key}.substr(iter->position(1), iter->length(1));
    //        o_hierarchy->push_back(token);
    //        o_display_key->append(token);
    //        o_display_key->append("|");
    //    }
    //    o_display_key->pop_back();  // remove last | character
    //
    //    auto it = std::regex_replace(o_display_key->begin(),
    //                                 o_display_key->begin(), o_display_key->end(),
    //                                 rg_remove_order_marker, "");
    //    o_display_key->resize(it - o_display_key->begin());
    //
    //    if (full_key.back() == '|') {
    //        throw std::invalid_argument(
    //                fmt::format("Invalid Key Name: {}", full_key));
    //    }
    //
    //    if (o_display_key->empty()) {
    //        throw std::invalid_argument(
    //                fmt::format("Invalid Generated Display Key Name: {} from full key {}",
    //                            *o_display_key, full_key));
    //    }
}
}  // namespace _configs
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
