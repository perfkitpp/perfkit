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
#include "fwd.hpp"
#include "handle.hpp"
#include "perfkit/common/functional.hxx"
#include "perfkit/common/math/matrix.hxx"
#include "perfkit/common/math/rectangle.hxx"

namespace perfkit::graphics {
using point2 = math::vec2i;
using size2 = math::vec2i;
using rectangle = math::rectangle;

enum class pixel_format {
    mono,    // 8-bit mono color
    f_mono,  // 32-bit floating point mono color
    rgb,     // 24-bit rgb color
    f_rgb,   // 96-bit rgb color
    rgba,    // 32-bit rgb color
    f_rgba,  // 128-bit rgb color
};

enum class modal_result {
    unavailable,  // couldn't pop up modal due to system issue.
    closed,       // user closed or pressed okay button in okay modal.
    yes,          // user pressed yes
    no,           // user pressed no
    expired,      // 'modal_expired' exception is thrown during iteration.
};

enum class modal_type {
    okay,          // simply show okey button
    yes_no,        // show yes/no button. closing modal will be treated as 'no'.
    yes_no_abort,  // show yes/no/abort button. closing modal will be treated as 'closed'
};

using window_proc_fn = perfkit::function<void(wdc*)>;
using texture_draw_fn = perfkit::function<void(dc*)>;

struct modal_expired : std::exception
{
};

/**
 * Generic vertex for material based mesh rendering
 */
struct vertex3_0
{
    math::vec3f position;
    math::vec2f uv0;
};

/**
 * Vertex for debug instance rendering
 */
struct vertex3_1
{
    math::vec3f position;
    math::vec3f normal;
    math::vec4b albedo;  // rgba order
    math::vec4b emission;
};

/**
 * Vertex for screen draw (especially for ImGUI forwarding)
 */
struct vertex2_0
{
    math::vec2f position;
    math::vec2f uv0;
};

/**
 * Material definition
 */
struct material
{
    texture_handle albedo = {};          // f_rgb
    texture_handle normal = {};          // f_rgb
    texture_handle metalic = {};         // f_mono
    texture_handle rouhness = {};        // f_mono
    texture_handle opacity = {};         // f_mono
    texture_handle emissive = {};        // f_rgb
    bool           binary_alpha = true;  // if set true, alpha channel will work as simple mask.
};

}  // namespace perfkit::graphics