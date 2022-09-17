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

#include "perfkit/detail/graphic-impl.hpp"

#include <cpph/std/algorithm>
#include <cpph/std/set>

#include <cpph/thread/thread_pool.hxx>
#include <cpph/utility/generic.hxx>

void perfkit::detail::window_impl::B_set_watching(bool value) noexcept
{
    relaxed(watching_, value);
    event_watch_state_.invoke(value);
}

auto perfkit::detail::window_impl::create_update_buffer(int width, int height, int channels) -> shared_ptr<char>
{
    auto p_buffer = buffer_pool_.checkout().share();
    auto view = p_buffer->mutable_buffer(width * height * channels);
    auto func_notify_buffer_update =
            [this, width, height, channels, p_buffer](auto) mutable {
                // Register as latest image buffer
                *latest_image_.lock() = const_image_buffer{
                        {width, height, channels}, {p_buffer, (char const*)p_buffer->data()}};

                // Notify backend for p_buffer content update
                event_buffer_update_.invoke(this, width, height, channels, p_buffer);
            };

    return shared_ptr<char>{view.data(), move(func_notify_buffer_update)};
}

void perfkit::detail::window_impl::set_quality(int percent)
{
    relaxed(quality_, clamp(percent, 0, 100));
    event_quality_change_.invoke(this, relaxed(quality_));
}

auto perfkit::detail::window_impl::event_watch_state_change() -> cpph::basic_event<cpph::spinlock, bool>::frontend
{
    return event_watch_state_.make_frontend();
}

auto perfkit::detail::window_impl::event_interaction() -> cpph::basic_event<cpph::spinlock, perfkit::windows::user_info_ptr, cpph::array_view<const perfkit::windows::input_event_data>>::frontend
{
    return event_interact_.make_frontend();
}

bool perfkit::detail::window_impl::is_being_watched() const
{
    return relaxed(watching_);
}

auto perfkit::detail::window_impl::latest_image() const noexcept -> perfkit::const_image_buffer
{
    return *latest_image_.lock();
}

void perfkit::detail::window_impl::B_notify_input(
        windows::user_info_wptr owner, pool_ptr<vector<windows::input_event_data>> work)
{
    static event_queue_worker worker{8 << 10};
    worker.post([this, w_self = weak_from_this(), w_owner = owner, work = move(work)] {
        auto self = w_self.lock();
        auto owner = w_owner.lock();
        if (not self || not owner) { return; }

        // Make all user interaction join to single thread.
        this->event_interact_.invoke(owner, view_array(*work));
    });
}

std::string perfkit::detail::window_impl::path() const noexcept
{
    return path_;
}

namespace {
using namespace cpph;
auto g_windows_()
{
    static locked<set<weak_ptr<perfkit::window>, std::owner_less<>>> _;
    return _.lock();
}
}  // namespace

auto perfkit::create_window(string path) -> shared_ptr<window>
{
    auto wnd = make_shared<detail::window_impl>(move(path));
    g_windows_()->insert(weak_ptr{wnd});
    detail::window_impl::event_register_.invoke(wnd.get());

    return wnd;
}

perfkit::detail::window_impl::~window_impl() noexcept
{
    g_windows_()->erase(weak_from_this());
    event_unregister_.invoke(this);
}

void perfkit::detail::window_impl::B_all_wnds(vector<shared_ptr<window_impl>>* all)
{
    auto lc_all = g_windows_();
    all->clear();
    all->reserve(lc_all->size());

    for (auto& wp : *lc_all)
        if (auto sp = static_pointer_cast<window_impl>(wp.lock()))
            all->emplace_back(move(sp));
}
