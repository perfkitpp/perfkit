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
#include <shared_mutex>
#include <unordered_map>

#include "configs-v2.hpp"
#include "cpph/memory/pool.hxx"
#include "cpph/refl/core.hxx"
#include "cpph/streambuf/string.hxx"
#include "cpph/thread/event_queue.hxx"
#include "cpph/utility/event.hxx"

namespace cpph {
using std::list;
}

namespace perfkit::v2 {
struct config_entity_context {
    friend class config_registry;

   public:
    config_base_wptr reference;
    size_t sort_order = 0;
    string full_key_cached;

   private:
    refl::shared_object_ptr _staged;
};

class config_registry::backend_t
{
    friend class config_registry;

   private:
    config_registry* _owner;
    string _name;

    // Unique id for this registry
    static inline atomic<uint64_t> _idgen = 0;
    config_registry_id_t _id = {++_idgen};

    // Flag for global repository registering
    std::once_flag _register_once_flag;
    atomic_bool _is_registered = false;
    atomic_bool _is_transient = false;
    atomic_bool _has_update = false;
    atomic_bool _has_expired_ref = false;
    bool _is_global = false;

    // Event queue for event joining
    event_queue _events{1024};

    // Data access lock. Data modification should only be done under this mutex's protection.
    mutable std::shared_mutex _mtx_access;

    // Actual modification fence
    atomic_size_t _fence = 0;
    atomic_size_t _sort_id_gen = 0;

    // All active config entities.
    std::unordered_map<config_id_t, config_entity_context> _configs;
    map<string, config_id_t> _key_id_table;

    // Set of queued updates
    vector<config_id_t> _refreshed_items;
    vector<config_id_t> _inplace_updates;

    // Item insertion/deletions management
    bool _flag_add_remove_notified = true;
    flat_map<config_base_wptr, tuple<size_t>, std::owner_less<>> _config_added;
    vector<config_base_wptr> _config_removed;

    // For events, Every event nodes can be reused.
    list<config_base_ptr> _free_evt_nodes;

   public:
    static inline event<config_registry_ptr> g_evt_registered;
    static inline event<weak_ptr<config_registry>> g_evt_unregistered;

    event<config_registry*, list<config_base_ptr> const&> evt_updated_entities;
    event<config_registry*> evt_structure_changed;

   private:
    void _register_to_global_repo();

    void _do_update();
    bool _handle_structure_update();
    bool _commit(config_base*, refl::shared_object_ptr);

   public:
    explicit backend_t(config_registry* self, string name, bool is_global)
            : _owner(self), _name(move(name)), _is_global(is_global) {}

   public:
    void _internal_notify_config_disposal() { release(_has_expired_ref, true); }
    bool _internal_commit_inplace(config_base*, refl::object_view_t);

   public:
    void bk_all_items(vector<config_base_ptr>*) const noexcept;
    bool bk_commit(config_base*, archive::if_reader* content);

    template <class AccessFn>
    void bk_access_value_shared(config_base* cfg, AccessFn&& fn)
    {
        cfg->_internal_read_lock();
        fn(cfg->_body.raw_data);
        cfg->_internal_read_unlock();
    }

    static void bk_enumerate_registries(std::vector<config_registry_ptr>* o_regs, bool include_unregistered = false) noexcept;
};

}  // namespace perfkit::v2