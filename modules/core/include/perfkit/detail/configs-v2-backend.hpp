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
#include "cpph/refl/core.hxx"
#include "cpph/thread/event_queue.hxx"

namespace perfkit::v2 {
struct config_entity_context {
    size_t sort_order;
};

class config_registry::backend_t
{
    friend class config_registry;

   private:
    config_registry* _self;
    string _name;

    // Flag for global repository registering
    std::once_flag _register_once_flag;

    // Event queue for event joining
    std::mutex _mtx_update;
    event_queue _events{1024};

    // Data access lock. Data modification should only be done under this mutex's protection.
    std::shared_mutex _mtx_access;

    // Actual modification fence
    atomic_size_t _fence = 0;
    size_t _sort_id_gen = 0;

    // All active config entities.
    std::unordered_map<uint64_t, config_entity_context> _configs;

    // Set of queued updates
    spinlock _mtx_refreshed;
    sorted_vector<uint64_t, refl::shared_object_ptr, std::owner_less<>> _refreshed;

    // Item insertion/deletions management
    bool _flag_add_remove_notified = false;
    vector<pair<config_base_wptr, bool /*1:add,0:remove*/>> _config_add_remove;

    // For events,

   public:
    //!
    static inline event<void(config_registry_ptr)> g_evt_registered;
    event<void(config_registry*, array_view<config_base_ptr>)> evt_updated_entities;
    event<void(config_registry*, array_view<config_base_ptr>)> evt_item_added;
    event<void(config_registry*, array_view<config_base_ptr>)> evt_item_removed;

    //!

   private:
    void _register_to_global_repo() {}

   public:
    explicit backend_t(config_registry* self, string name) : _self(self), _name(move(name)) {}

   public:
    void find_key(string_view display_key, string* out_full_key) {}
    void all_items(vector<config_base_ptr>*) const noexcept {}

   public:
    static void enumerate_registries(vector<shared_ptr<config_registry>>* o_regs, bool filter_complete = false) noexcept {}
    static auto find_registry(string_view name) noexcept -> shared_ptr<config_registry> { return {}; }
};

}  // namespace perfkit::v2