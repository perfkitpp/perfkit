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
#include <bitset>
#include <cpph/std/memory>
#include <cpph/std/string>
#include <cpph/std/variant>

#include <cpph/utility/array_view.hxx>
#include <cpph/utility/event.hxx>

namespace perfkit {
using namespace cpph;

namespace windows {
// clang-format off
enum class keycode {
    none,

    space, enter, backspace, del, ins, pgup, pgdn, home, end, escape, tab, caps_lock,
    shift_l, shift_r, alt_l, alt_r, ctrl_l, ctrl_r,

    //      [          ]      /         \\             `      -      =    .      ,          ;     '
    bracket_l, bracket_r, slash, backslash, counter_apos, minus, equal, dot, comma, semicolon, apos,

    arrow_base,
    arrow_up = arrow_base, arrow_right, arrow_down, arrow_left,

    function_key_base,
    f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12,

    alphabet_base,
    a = alphabet_base, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z,

    n_base,
    n0 = n_base, n1, n2, n3, n4, n5, n6, n7, n8, n9,

    num_base,
    num_0 = num_base, num_1, num_2, num_3, num_4, num_5, num_6, num_7, num_8, num_9,
};
// clang-format on

inline keycode operator+(keycode code, int offset) noexcept { return keycode(int(code) + offset); }

struct mouse_event_info {
    int x, y;
    bool clk_left   : 1;
    bool clk_middle : 1;
    bool clk_right  : 1;
    bool key_ctrl   : 1;
    bool key_alt    : 1;
    bool key_shift  : 1;
};

struct keyboard_event_info {
    keycode code;
    int code_raw;
    bool key_ctrl  : 1;
    bool key_alt   : 1;
    bool key_shift : 1;
};

namespace events {
// clang-format off
struct mouse_click : mouse_event_info {};
struct mouse_down : mouse_event_info {};
struct mouse_up : mouse_event_info {};
struct mouse_move : mouse_event_info {};
struct mouse_enter : mouse_event_info {};
struct mouse_leave : mouse_event_info {};
struct key_down : keyboard_event_info {};
struct key_up : keyboard_event_info {};
struct key : keyboard_event_info { string key_value; }; // For IME input events ...
struct on_focus {};
struct on_blur {};
// clang-format on
}  // namespace events

struct user_info {
    size_t id = 0;
    string name;
};

using user_info_ptr = shared_ptr<user_info const>;
using user_info_wptr = weak_ptr<user_info const>;

using input_event_data = variant<
        monostate,
        events::mouse_click,
        events::mouse_down,
        events::mouse_up,
        events::mouse_move,
        events::mouse_enter,
        events::mouse_leave,
        events::key_down,
        events::key_up,
        events::key,
        events::on_focus,
        events::on_blur,
        nullptr_t>;

}  // namespace windows

/**
 * Holds image contents
 */
struct image_info {
    int width = 0;
    int height = 0;
    int channels = 0;

    int buflen() const noexcept { return width * height * channels; }
    int empty() const noexcept { return width == 0 || height == 0 || channels == 0; }
};
struct image_buffer : image_info {
    shared_ptr<char> data;
};
struct const_image_buffer : image_info {
    shared_ptr<char const> data;
};

/**
 * Control to window
 */
class window : public enable_shared_from_this<window>
{
   public:
    using watch_event_type = event<bool>;
    using input_events_view_type = array_view<windows::input_event_data const>;
    using interaction_event_type = event<windows::user_info_ptr, input_events_view_type>;

   public:
    virtual ~window() noexcept = default;

    /**
     * Check if this window is being watched
     * @return
     */
    virtual bool is_being_watched() const = 0;

    /**
     * Get path to this window instance.
     *
     * May not changed.
     */
    virtual string path() const noexcept = 0;

    /**
     * Create image update buffer. Buffer will automatically committed when submitted.
     *
     * Size of allocated buffer is width*height*channels
     *
     * channels == 1: mono
     * channels == 2: mono, alpha
     * channels == 3: bgr
     * channels == 4: bgra
     */
    virtual auto create_update_buffer(int width, int height, int channels) -> shared_ptr<char> = 0;

    /**
     * Set image transfer quality
     *
     * 0 to 100. Default is 50
     */
    virtual void set_quality(int percent) = 0;

    /**
     * Listen to watch state change event.
     *
     * Can be invoked from any thread
     */
    virtual auto event_watch_state_change() -> watch_event_type::frontend = 0;

    /**
     * Listen to user input event.
     */
    virtual auto event_interaction() -> interaction_event_type::frontend = 0;
};

auto create_window(string path) -> shared_ptr<window>;
}  // namespace perfkit
