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
#include "cpph/event.hxx"
#include "cpph/memory/pool.hxx"
#include "cpph/refl/archive/msgpack-reader.hxx"
#include "cpph/refl/core.hxx"
#include "cpph/streambuf/string.hxx"
#include "cpph/thread/event_queue.hxx"

namespace cpph {
using std::list;
}

namespace perfkit::v2 {
struct config_entity_context {
    size_t sort_order = 0;
    pool_ptr<string> next_value;
};

class config_registry::backend_t
{
    friend class config_registry;

   private:
    config_registry* _self;
    string _name;

    // Unique id for this registry
    static inline atomic<uint64_t> _idgen = 0;
    config_registry_id_t _id = {++_idgen};

    // Flag for global repository registering
    std::once_flag _register_once_flag;

    // Event queue for event joining
    event_queue _events{1024};

    // Data access lock. Data modification should only be done under this mutex's protection.
    std::shared_mutex _mtx_access;

    // Actual modification fence
    atomic_size_t _fence = 0;
    atomic_size_t _sort_id_gen = 0;

    // Serialization Management
    pool<string> _pool_update_buffer;

    // All active config entities.
    std::unordered_map<config_id_t, config_entity_context> _configs;

    // Set of queued updates
    spinlock _mtx_refreshed;
    vector<config_id_t> _refreshed;

    // Item insertion/deletions management
    bool _flag_add_remove_notified = false;
    sorted_vector<config_base_wptr, tuple<size_t, string>, std::owner_less<>> _config_added;
    vector<config_base_wptr> _config_removed;

    // For events, Every event nodes can be reused.
    list<config_base_ptr> _free_evt_nodes;

   public:
    static inline event<void(config_registry_ptr)> g_evt_registered;
    static inline event<void(config_registry_ptr)> g_evt_unregistered;

    event<void(config_registry*, list<config_base_ptr> const&)> evt_updated_entities;
    event<void(config_registry*, list<config_base_ptr> const&)> evt_update_failed_entities;
    event<void(config_registry*, list<config_base_ptr> const&)> evt_item_added;
    event<void(config_registry*, list<config_base_ptr> const&)> evt_item_removed;

   private:
    void _register_to_global_repo() {}
    void _commit_serial(config_id_t, pool_ptr<string>);

   public:
    explicit backend_t(config_registry* self, string name) : _self(self), _name(move(name)) {}

   public:
    void find_key(string_view display_key, string* out_full_key) {}
    void all_items(vector<config_base_ptr>*) const noexcept {}
    void bk_commit(config_id_t, string* io_msgpack_content);

   public:
    static void enumerate_registries(vector<shared_ptr<config_registry>>* o_regs, bool filter_complete = false) noexcept {}
    static auto find_registry(string_view name) noexcept -> shared_ptr<config_registry> { return {}; }
};

}  // namespace perfkit::v2