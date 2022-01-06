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

#pragma once
#include <array>
#include <vector>

#include <assert.h>

#include "perfkit/graphics/types.hpp"

namespace perfkit::graphics::detail {
using std::array;
using std::vector;

constexpr size_t to_index(resource_type t) noexcept
{
    return (size_t)(t)-1;
}

class handle_registry
{
   public:
    struct node
    {
        uint16_t index    = 0;
        uint32_t hash_gen = 0;
        bool checkout     = false;
    };

    struct slot
    {
        vector<uint16_t> free_keys;
        vector<node> keys;
    };

   public:
    template <typename Handle_>
    Handle_ checkout() noexcept
    {
        Handle_ result;
        handle_data* data = &result;

        slot& slot  = _slots.at(to_index(Handle_::type));
        data->_type = Handle_::type;

        if (not slot.free_keys.empty())
        {
            auto idx = slot.free_keys.back();
            slot.free_keys.pop_back();

            auto* node = &slot.keys.at(idx);
            assert(not node->checkout);
            assert(node->index == idx);

            node->checkout = true;

            data->_index = idx;
            data->_hash  = ++node->hash_gen;
        }
        else if (slot.keys.size() < ~uint16_t{})
        {
            auto idx  = uint16_t(slot.keys.size());
            auto node = &slot.keys.emplace_back();

            node->checkout = true;
            node->index    = idx;

            data->_index = idx;
            data->_hash  = ++node->hash_gen;
        }
        else
        {
            data->_type = resource_type::invalid;
        }

        return result;
    }

    void checkin(handle_data const& data)
    {
        slot& slot = _slots.at(data.resource_index());

        auto node = &slot.keys.at(data._index);
        assert(node->hash_gen == data._hash);
        assert(node->checkout);
        assert(node->index == data._index);

        node->checkout = false;
        slot.free_keys.push_back(data._index);
    }

   private:
    array<slot, size_t(resource_type::MAX_) - 1> _slots;
};
}  // namespace perfkit::graphics::detail