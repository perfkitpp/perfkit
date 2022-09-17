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
#include <cpph/std/vector>
#include <utility>

#include <cpph/container/buffer.hxx>
#include <cpph/memory/pool.hxx>
#include <cpph/thread/locked.hxx>

#include "graphic.hpp"

namespace perfkit::detail {

/**
 * May all backend classes can access to this class.
 */
class window_impl : public window
{
    friend auto perfkit::create_window(string path) -> shared_ptr<window>;

   private:
    string const path_;
    watch_event_type event_watch_state_;

    pool<flex_buffer> buffer_pool_;

    atomic_int quality_ = 50;
    atomic_bool watching_ = false;

    locked<const_image_buffer> latest_image_;

    //! Backend should notify interaction event through this.
    interaction_event_type event_interact_;

   private:
    static inline event<window_impl*, int, int, int, shared_ptr<flex_buffer>&> event_buffer_update_;
    static inline event<window_impl*, int> event_quality_change_;
    static inline event<window_impl*> event_register_;
    static inline event<window_impl*> event_unregister_;

   public:  // Backend interfaces
    static inline decltype(event_buffer_update_)::frontend const B_on_buffer_update = event_buffer_update_.make_frontend();
    static inline decltype(event_quality_change_)::frontend const B_on_quality_change = event_quality_change_.make_frontend();
    static inline decltype(event_register_)::frontend const B_on_register = event_register_.make_frontend();
    static inline decltype(event_unregister_)::frontend const B_on_unregister = event_unregister_.make_frontend();

    static void B_all_wnds(vector<shared_ptr<window_impl>>*);

   public:
    explicit window_impl(string path) : path_(std::move(path)) {}
    ~window_impl() noexcept;

    bool is_being_watched() const override;

    auto create_update_buffer(int width, int height, int channels) -> shared_ptr<char> override;

    void set_quality(int percent) override;

    auto event_watch_state_change() -> watch_event_type::frontend override;

    auto event_interaction() -> interaction_event_type::frontend override;

    string path() const noexcept override;

   public:
    int B_get_quality() const noexcept { return relaxed(quality_); }
    void B_set_watching(bool value) noexcept;
    void B_notify_input(windows::user_info_wptr, pool_ptr<vector<windows::input_event_data>>);
    auto latest_image() const noexcept -> const_image_buffer;
};

}  // namespace perfkit::detail
