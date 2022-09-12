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

#include <cpph/container/buffer.hxx>
#include <cpph/memory/pool.hxx>

#include "graphic.hpp"

namespace perfkit::detail {
/**
 * May all backend classes can access to this class.
 */
class window_impl : public window
{
   private:
    watch_event_type event_watch_state_;

    pool<flex_buffer> buffer_pool_;

    atomic_int quality_ = 50;
    atomic_bool watching_ = false;

   private:
    static inline event<window_impl*, int, int, int, pool_ptr<flex_buffer>&> event_buffer_update_;
    static inline event<window_impl*, int> event_quality_change_;

   public:  // Backend interfaces
    static inline decltype(event_buffer_update_)::frontend const B_on_buffer_update = event_buffer_update_.make_frontend();
    static inline decltype(event_quality_change_)::frontend const B_on_quality_change = event_quality_change_.make_frontend();

    //! Backend should notify interaction event through this.
    interaction_event_type B_on_interact;

   public:
    bool is_being_watched() const override
    {
        return relaxed(watching_);
    }

    auto create_update_buffer(int width, int height, int channels) -> shared_ptr<char> override
    {
        auto p_buffer = buffer_pool_.checkout();
        auto view = p_buffer->mutable_buffer(width * height * channels);
        auto func_notify_buffer_update =
                [this, width, height, channels, p_buffer = move(p_buffer)](auto) mutable {
                    // Notify backend for p_buffer content update
                    event_buffer_update_.invoke(this, width, height, channels, p_buffer);
                };

        return shared_ptr<char>{view.data(), move(func_notify_buffer_update)};
    }

    void set_quality(int percent) override
    {
        relaxed(quality_, clamp(percent, 0, 100));
        event_quality_change_.invoke(this, relaxed(quality_));
    }

    auto event_watch_state_change() -> watch_event_type::frontend override
    {
        return event_watch_state_.make_frontend();
    }

    auto event_interaction() -> interaction_event_type::frontend override
    {
        return B_on_interact.make_frontend();
    }

   public:
    int B_get_quality() const noexcept { return relaxed(quality_); }
    void B_set_watching(bool value) noexcept;
};

}  // namespace perfkit::detail
