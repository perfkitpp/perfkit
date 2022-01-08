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

//
// Created by Seungwoo on 2021-08-25.
//
#include "perfkit/detail/configs.hpp"

#include <cassert>
#include <condition_variable>
#include <regex>
#include <utility>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>
#include <spdlog/spdlog.h>

#include "perfkit/common/format.hxx"
#include "perfkit/common/hasher.hxx"
#include "perfkit/common/macros.hxx"
#include "perfkit/common/utility/singleton.hxx"
#include "perfkit/perfkit.h"

#define CPPH_LOGGER() perfkit::glog()

namespace views = ranges::views;

namespace perfkit::detail {
auto _all_repos()
{
    static config_registry::container _inst;
    static spinlock _lock;
    return std::make_pair(&_inst, std::unique_lock{_lock});
}
}  // namespace perfkit::detail

// namespace perfkit::detail

auto perfkit::config_registry::create(std::string name, std::type_info const* schema)
        -> shared_ptr<config_registry>
{
    auto [all, _] = detail::_all_repos();
    CPPH_DEBUG("Creating new config registry {}", name);

    auto rg_ptr           = new config_registry{std::move(name)};
    rg_ptr->_schema_class = schema;

    shared_ptr<config_registry> rg{rg_ptr};
    auto [it, is_new] = all->try_emplace(rg->name(), rg);

    if (not is_new) { throw std::logic_error{"CONFIG REGISTRY NAME MUST BE UNIQUE!!!"}; }

    return rg;
}

auto perfkit::config_registry::share(std::string_view name, std::type_info const* schema)
        -> std::shared_ptr<perfkit::config_registry>
{
    if (auto [all, _] = detail::_all_repos(); 1)
    {
        auto it = all->find(name);

        if (it != all->end())
        {
            auto repo = it->second.lock();
            if (repo->bk_schema_class() != schema)
                throw configs::schema_mismatch{"schema must match!"};

            return repo;
        }
    }

    return create(std::string{name}, schema);
}

perfkit::config_registry::~config_registry() noexcept
{
    CPPH_DEBUG("destroying config registry {}", name());
    on_destroy.invoke(this);

    auto [all, _] = detail::_all_repos();
    all->erase(all->find(name()));
}

auto perfkit::config_registry::bk_enumerate_registries(bool filter_complete) noexcept
        -> std::vector<std::shared_ptr<config_registry>>
{
    std::vector<std::shared_ptr<config_registry>> out;
    {
        using namespace ranges;
        auto [all, _] = detail::_all_repos();

        auto ptrs = views::all(*all)
                  | views::transform(
                            [](auto&& o) {
                                return o.second.lock();
                            });

        if (not filter_complete)
        {
            out.assign(ptrs.begin(), ptrs.end());
        }
        else
        {
            auto filtered = ptrs
                          | views::filter(
                                    [](auto&& sp) {
                                        return sp->_initially_updated();
                                    });

            out.assign(filtered.begin(), filtered.end());
        }
    }

    return out;
}

auto perfkit::config_registry::bk_find_reg(std::string_view name) noexcept
        -> std::shared_ptr<perfkit::config_registry>
{
    auto [all, _] = detail::_all_repos();
    auto it       = all->find(name);

    if (it == all->end()) { return nullptr; }
    auto ptr = it->second.lock();

    assert(ptr != nullptr && "LOGIC ERROR: POINTER MUST NOT BE NULL HERE!");
    return ptr;
}

namespace perfkit::configs::_io {
auto _loaded()
{
    static json _all;
    static spinlock _lock;

    return std::make_pair(&_all, std::unique_lock{_lock});
}

json fetch_changes(std::string_view reg_name)
{
    auto [js, _] = _io::_loaded();
    auto it      = js->find(reg_name);
    if (it == js->end()) { return {}; }

    return *it;
}

void queue_changes(shared_ptr<config_registry> rg, json patch)
{
    if (glog()->should_log(spdlog::level::debug))
    {
        CPPH_DEBUG("applying changes to category '{}', content: {}\n", rg->name(), patch.dump(2));
    }

    // apply all update
    for (auto& [disp_key, value] : patch.items())
    {
        auto full_key = rg->bk_find_key(disp_key);
        if (full_key.empty())
        {
            CPPH_WARN("key {} does not exist on category {}", disp_key, rg->name());
            continue;
        }

        auto it = rg->bk_all().find(full_key);
        if (it == rg->bk_all().end())
        {
            CPPH_CRITICAL(
                    "rg: {}, disp key: {}, full_key: {}, is not valid.",
                    rg->name(), disp_key, full_key);
            continue;
        }

        if (not it->second->can_import())
            continue;

        rg->bk_queue_update_value(full_key, std::move(value));
    }
}
}  // namespace perfkit::configs::_io

static auto import_export_reenter_lock()
{
    static std::mutex mt;
    return std::unique_lock{mt};
}

bool perfkit::configs::import_from(json const& data)
{
    if (not data.is_object())
        return false;

    auto _l{import_export_reenter_lock()};
    (void)_l;

    // 1. Replace cache as new configuration object
    // 2. Queue all changes for each registry
    {
        auto [js, _] = _io::_loaded();
        *js          = data;
    }

    auto registries = config_registry::bk_enumerate_registries();
    for (auto const& registry : registries)
    {
        auto it = data.find(registry->name());
        if (it == data.end()) { continue; }

        _io::queue_changes(registry, *it);
    }

    return true;
}

void perfkit::config_registry::export_to(nlohmann::json* category)
{
    category->clear();

    for (auto const& [full_key, config] : bk_all())
    {
        if (config->can_export())
            category->emplace(config->display_key(), config->serialize());
    }
}

void perfkit::config_registry::import_from(nlohmann::json obj)
{
    configs::_io::queue_changes(shared_from_this(), std::move(obj));
}

perfkit::json perfkit::configs::export_all()
{
    auto _l{import_export_reenter_lock()};
    (void)_l;

    // 1. Iterate all registry instances, and compose configuration list
    // 2. Merge configuration list into existing loaded cache, then return it.
    auto regs = config_registry::bk_enumerate_registries();

    CPPH_TRACE("exporting {} config repositories ...", regs.size());
    json exported;

    // merge onto existing (will not affect existing cache)
    json current;
    {
        auto [js, _] = _io::_loaded();
        current      = *js;
    }

    for (auto const& rg : regs)
    {
        // if there's no target currently, export as template
        bool do_export = not current.contains(rg->name());

        // if registry is updated after creation, export unconditionally.
        do_export |= rg->_initially_updated();

        if (not do_export) { continue; }  // the first update() not called yet.
        auto category = &exported[rg->name()];

        rg->export_to(category);
    }

    for (auto& item : exported.items())
    {
        current[item.key()] = std::move(item.value());
    }

    return current;
}

perfkit::event<perfkit::config_registry*>& perfkit::configs::on_new_config_registry()
{
    constexpr auto p = [] {};  // give uniqueness for this singleton
    return singleton<perfkit::event<perfkit::config_registry*>, decltype(p)>.get();
}

bool perfkit::config_registry::update()
{
    if (not _initial_update_done.load(std::memory_order_consume))
    {
        CPPH_DEBUG("registry '{}' instantiated after loading configurations", name());

        auto patch = configs::_io::fetch_changes(name());
        if (not patch.empty())
        {
            configs::_io::queue_changes(shared_from_this(), std::move(patch));
        }

        // exporting configs only allowed after first update.
        _initial_update_done.store(true, std::memory_order_release);

        configs::on_new_config_registry().invoke(this);
    }

    if (std::unique_lock _l{_update_lock})
    {
        if (_pending_updates[0].empty()) { return false; }  // no update.

        auto& update = _pending_updates[1];

        bool has_valid_update = false;
        update                = _pending_updates[0];
        _pending_updates[0].clear();

        for (auto ptr : update)
        {
            auto r_desrl = ptr->_try_deserialize(ptr->_cached_serialized);

            if (!r_desrl)
            {
                CPPH_ERROR("parse failed: '{}' <- {}", ptr->display_key(), ptr->_cached_serialized.dump());
                ptr->_serialize(ptr->_cached_serialized, ptr->_raw);  // roll back
            }
            else
            {
                has_valid_update |= true;
            }
        }

        _l.unlock();

        if (has_valid_update && not on_update.empty()) { on_update.invoke(this, update); }
    }

    return true;
}

void perfkit::config_registry::_put(std::shared_ptr<detail::config_base> o)
{
    CPPH_TRACE("new config: {} ({})", o->full_key(), o->display_key());

    if (_initially_updated())
    {
        CPPH_CRITICAL(
                "putting new configuration '{}' after first update of registry '{}'...",
                o->display_key(), name());
        throw std::logic_error{""};
    }

    if (auto it = _entities.find(o->full_key()); it != _entities.end())
    {
        throw std::invalid_argument("Argument name MUST be unique within category!!!");
    }

    if (not bk_find_key(o->display_key()).empty())
    {
        throw std::invalid_argument(fmt::format(
                "Duplicated Display Key Found: \n\t{} (from full key {})",
                o->display_key(), o->full_key()));
    }

    _disp_keymap.try_emplace(o->display_key(), o->full_key());
    _entities.try_emplace(o->full_key(), o);

    // update schema hash
    _schema_hash = {hasher::fnv1a_64(o->full_key(), _schema_hash.value)};

    // TODO: throw error if flag belongs to disposable registry
    if (auto attr = &o->attribute(); attr->contains("is_flag"))
    {
        std::vector<std::string> bindings;
        if (auto it = attr->find("flag_binding"); it != attr->end())
        {
            bindings = it->get<std::vector<std::string>>();
        }
        else
        {
            using namespace ranges;
            bindings.emplace_back(
                    o->display_key()
                    | views::transform([](auto c) { return c == '|' ? '.' : c; })
                    | to<std::string>());
        }

        for (auto& binding : bindings)
        {
            static std::regex match{R"RG(^(((?!no-)[^N-][\w-\.]*)|N[\w-\.]+))RG"};
            if (!std::regex_match(binding, match)
                || binding.find_first_of(' ') != ~size_t{}
                || binding == "help" || binding == "h")
            {
                throw configs::invalid_flag_name(
                        fmt::format("invalid flag name: {}", binding));
            }

            auto [_, is_new] = configs::_flags().try_emplace(std::move(binding), o);
            if (!is_new)
            {
                throw configs::duplicated_flag_binding{
                        fmt::format("Binding name duplicated: {}", binding)};
            }
        }
    }
}

std::string_view perfkit::config_registry::bk_find_key(std::string_view display_key)
{
    if (auto it = _disp_keymap.find(display_key); it != _disp_keymap.end())
    {
        return it->second;
    }
    else
    {
        return {};
    }
}

bool perfkit::config_registry::bk_queue_update_value(std::string_view full_key, json value)
{
    auto _ = _access_lock();

    auto it = _entities.find(full_key);
    if (it == _entities.end()) { return false; }

    // to prevent value ignorance on contiguous load-save call without apply_changes(),
    // stores cache without validation.
    it->second->_cached_serialized = std::move(value);
    it->second->_fence_serialized  = ++it->second->_fence_modified;

    // push unique element
    auto updates  = _pending_updates + 0;
    auto conf_ptr = it->second.get();
    if (auto it_up = std::find(updates->begin(), updates->end(), conf_ptr);
        it_up == updates->end())
        updates->push_back(conf_ptr);

    return true;
}

perfkit::config_registry::config_registry(std::string name)
        : _name(std::move(name)) {}

perfkit::detail::config_base::config_base(
        config_registry* owner,
        void* raw, std::string full_key,
        perfkit::detail::config_base::deserializer fn_deserial,
        perfkit::detail::config_base::serializer fn_serial,
        nlohmann::json&& attribute)
        : _owner(owner),
          _full_key(std::move(full_key)),
          _raw(raw),
          _attribute(std::move(attribute)),
          _deserialize(std::move(fn_deserial)),
          _serialize(std::move(fn_serial))
{
    static std::regex rg_trim_whitespace{R"((?:^|\|?)\s*(\S?[^|]*\S)\s*(?:\||$))"};
    static std::regex rg_remove_order_marker{R"(\+[^|]+\|)"};

    _display_key.reserve(_full_key.size());
    for (std::sregex_iterator end{}, it{_full_key.begin(), _full_key.end(), rg_trim_whitespace};
         it != end;
         ++it)
    {
        if (!it->ready()) { throw "Invalid Token"; }
        _display_key.append(it->str(1));
        _display_key.append("|");
    }
    _display_key.pop_back();  // remove last | character

    auto it = std::regex_replace(_display_key.begin(),
                                 _display_key.begin(), _display_key.end(),
                                 rg_remove_order_marker, "");
    _display_key.resize(it - _display_key.begin());

    if (_full_key.back() == '|')
    {
        throw std::invalid_argument(
                fmt::format("Invalid Key Name: {}", _full_key));
    }

    if (_display_key.empty())
    {
        throw std::invalid_argument(
                fmt::format("Invalid Generated Display Key Name: {} from full key {}",
                            _display_key, _full_key));
    }

    _split_categories(_display_key, _categories);
}

bool perfkit::detail::config_base::_try_deserialize(nlohmann::json const& value)
{
    if (_deserialize(value, _raw))
    {
        _fence_modified.fetch_add(1, std::memory_order_relaxed);
        _dirty = true;

        _latest_marshal_failed.store(false, std::memory_order_relaxed);
        return true;
    }
    else
    {
        _latest_marshal_failed.store(true, std::memory_order_relaxed);
        return false;
    }
}

nlohmann::json perfkit::detail::config_base::serialize()
{
    nlohmann::json copy;
    return serialize(copy), copy;
}

void perfkit::detail::config_base::serialize(nlohmann::json& copy)
{
    if (auto nmodify = num_modified(); _fence_serialized != nmodify)
    {
        auto _lock = _owner->_access_lock();
        _serialize(_cached_serialized, _raw);
        _fence_serialized = nmodify;
        copy              = _cached_serialized;
    }
    else
    {
        auto _lock = _owner->_access_lock();
        copy       = _cached_serialized;
    }
}

void perfkit::detail::config_base::serialize(std::function<void(nlohmann::json const&)> const& fn)
{
    auto _lock = _owner->_access_lock();

    if (auto nmodify = num_modified(); _fence_serialized != nmodify)
    {
        _serialize(_cached_serialized, _raw);
        _fence_serialized = nmodify;
    }

    fn(_cached_serialized);
}

void perfkit::detail::config_base::request_modify(nlohmann::json js)
{
    _owner->bk_queue_update_value(std::string(full_key()), std::move(js));
}

void perfkit::detail::config_base::_split_categories(std::string_view view, std::vector<std::string_view>& out)
{
    out.clear();

    // always suppose category is valid.
    for (size_t i = 0; i < view.size();)
    {
        if (view[i] == '|')
        {
            out.push_back(view.substr(0, i));
            view = view.substr(i + 1);
            i    = 0;
        }
        else
        {
            ++i;
        }
    }

    out.push_back(view);  // last segment.
}

bool perfkit::configs::watcher::_check_update_and_consume(
        perfkit::detail::config_base* ptr) const
{
    auto* fence = &_table[ptr];
    if (*fence != ptr->num_modified())
        return *fence = ptr->num_modified(), true;
    else
        return false;
}
