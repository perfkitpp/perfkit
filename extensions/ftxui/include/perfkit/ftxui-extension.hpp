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
#include <chrono>
#include <string_view>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

#include "perfkit/common/array_view.hxx"
#include "perfkit/common/circular_queue.hxx"
#include "perfkit/detail/fwd.hpp"
#include "perfkit/terminal.h"

namespace ftxui {
class ScreenInteractive;
};

namespace perfkit_ftxui {
/**
 * Custom subscription event handler
 */
class if_subscriber
{
   public:
    struct update_param_type
    {
        std::string_view                      block_name;

        uint64_t                              hash = {};
        perfkit::array_view<std::string_view> hierarchy;
        std::string_view                      name;
    };

   public:
    virtual ~if_subscriber() = default;

    virtual bool on_update(
            update_param_type const&           param_type,
            perfkit::trace_variant_type const& value)
            = 0;

    virtual void on_end(update_param_type const& param_type) = 0;
};

using namespace std::literals;
static inline const auto EVENT_POLL = ftxui::Event::Special("POLL");

ftxui::Component         config_browser();
ftxui::Component         trace_browser(std::shared_ptr<if_subscriber> m);

class string_queue
{
   public:
    virtual bool getline(std::string&, std::chrono::milliseconds) = 0;
    bool         try_getline(std::string& s) { return this->getline(s, 0ms); }
    virtual ~string_queue() = default;
};

ftxui::Component command_input(std::shared_ptr<string_queue>*                 out_supplier,
                               std::weak_ptr<perfkit::util::command_registry> support,
                               std::string                                    prompt = {});
ftxui::Component event_dispatcher(ftxui::Component, const ftxui::Event& evt_type = EVENT_POLL);

ftxui::Component PRESET(
        std::shared_ptr<string_queue>*                 out_commands,
        std::weak_ptr<perfkit::util::command_registry> command_support,
        std::shared_ptr<if_subscriber>                 subscriber);

class worker_info_t;
using kill_switch_ty = std::shared_ptr<worker_info_t>;

/**
 * @param screen
 * @param root_component
 * @param poll_interval Interval to generate every Event::Special("POLL")
 * @return kill switch. To stop worker threads gently, release given pointer.
 */
kill_switch_ty launch_async_loop(
        ftxui::ScreenInteractive* screen,
        ftxui::Component          root_component,
        std::chrono::milliseconds poll_interval = 33ms);

bool is_alive(worker_info_t const* kill_switch);

}  // namespace perfkit_ftxui