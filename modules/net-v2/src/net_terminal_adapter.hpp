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

//
// Created by ki608 on 2022-03-19.
//

#pragma once
#include "cpph/functional.hxx"
#include "perfkit/detail/fwd.hpp"

namespace cpph::rpc {
class session_profile;
class session_group;
}

namespace perfkit::net {
using std::shared_ptr;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::vector;
using std::weak_ptr;
}  // namespace perfkit::net

namespace asio {
class io_context;
}

namespace perfkit::net {
class if_net_terminal_adapter
{
   public:
    virtual ~if_net_terminal_adapter() noexcept = default;

    virtual bool has_basic_access(rpc::session_profile const*) const = 0;
    virtual bool has_admin_access(rpc::session_profile const*) const = 0;

    void         post_to_event_procedure(function<void()>&& invocable);

    template <typename Callable, typename... Args>
    void post_weak(weak_ptr<void> weak, Callable&& callable, Args&&... args)
    {
        post_to_event_procedure(
                bind_front_weak(std::move(weak), std::forward<Callable>(callable), std::forward<Args>(args)...));
    }

    template <typename Callable, typename... Args>
    void post(Callable&& callable, Args&&... args)
    {
        post_to_event_procedure(bind(std::forward<Callable>(callable), std::forward<Args>(args)...));
    }

    //
    virtual asio::io_context*   event_proc() = 0;
    virtual rpc::session_group* rpc() = 0;

   public:
    auto fn_basic_access() const { return bind_front(&if_net_terminal_adapter::has_basic_access, this); }
    auto fn_admin_access() const { return bind_front(&if_net_terminal_adapter::has_admin_access, this); }
};
}  // namespace perfkit::net
