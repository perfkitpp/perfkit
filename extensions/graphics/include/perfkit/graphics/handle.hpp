/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2021. Seungwoo Kang
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
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "fwd.hpp"

namespace perfkit::graphics {
enum class resource_type : uint8_t
{
    invalid,
    texture,
    window,
    mesh,
    material,
    MAX_,
};

struct handle_data
{
   public:
    bool is_valid() const noexcept
    {
        return resource_type::invalid < _type && _type < resource_type::MAX_;
    }

    operator bool() const noexcept
    {
        return is_valid();
    }

   public:
    uint16_t index() const noexcept { return _index; }
    uint32_t hash() const noexcept { return _hash; }
    uint8_t resource_index() const noexcept { return (uint8_t)_type - 1; }

   public:
    resource_type _type = {};

    uint8_t _padding = {};

    uint16_t _index = {};
    uint32_t _hash  = {};
};

/**
 * @tparam Label_ Only used for type identification
 */
template <resource_type Resource_>
struct _handle_base : handle_data
{
    static constexpr auto type = Resource_;

    static _handle_base from(handle_data data)
    {
        if (not data)
            throw std::logic_error("invalid type!");

        if (type != data._type)
            throw std::logic_error("type mismatch!");

        return *(_handle_base*)&data;
    }
};

static_assert(sizeof _handle_base<resource_type::invalid>{} == 8);

using texture_handle  = _handle_base<resource_type::texture>;
using window_handle   = _handle_base<resource_type::window>;
using mesh_handle     = _handle_base<resource_type::mesh>;
using material_handle = _handle_base<resource_type::material>;
}  // namespace perfkit::graphics
