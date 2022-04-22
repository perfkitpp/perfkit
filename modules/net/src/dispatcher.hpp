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

//
// Created by ki608 on 2021-11-21.
//

#pragma once
#include <functional>
#include <memory>

#include <cpph/event.hxx>
#include <nlohmann/json.hpp>
#include <perfkit/extension/net.hpp>

namespace perfkit::terminal::net::detail {
class basic_dispatcher_impl;
}

namespace perfkit::terminal::net {
class dispatcher
{
    using impl_ptr = std::unique_ptr<detail::basic_dispatcher_impl>;
    using recv_archive_type = nlohmann::json;
    using send_archive_type = nlohmann::json;

   public:
    explicit dispatcher(terminal_init_info const& init_info);
    ~dispatcher();

    template <typename MsgTy_,
              typename HandlerFn_,
              typename = std::enable_if_t<std::is_invocable_v<HandlerFn_, MsgTy_>>>
    void on_recv(HandlerFn_&& handler)
    {
        // TODO: change parser as SAX parser one with reflection, to minimize heap usage
        _register_recv(
                MsgTy_::ROUTE,
                [fn = std::forward<HandlerFn_>(handler)](recv_archive_type const& message) {
                    try {
                        fn(message.get<MsgTy_>());
                        return true;
                    } catch (std::exception&) {
                        return false;  // failed to parse message ...
                    }
                });
    }

    template <typename MsgTy_>
    void send(MsgTy_ const& message)
    {
        // TODO: change serializer to use builder, which doesn't need to be marshaled into
        //        json object to be dumped to string.
        _send(MsgTy_::ROUTE, ++_fence, &message,
              [](send_archive_type* archive, void const* data) {
                  *archive = *(MsgTy_ const*)data;
              });
    }

    void dispatch(perfkit::function<void()>);
    void post(perfkit::function<void()>);
    void close_all();

    std::pair<size_t, size_t>
    num_bytes_in_out() const noexcept;

    perfkit::event<int>& on_new_connection();
    perfkit::event<>& on_no_connection();

    void launch();

   private:
    void _register_recv(
            std::string route,
            std::function<bool(recv_archive_type const& parameter)>);

    void _send(
            std::string_view route,
            int64_t fence,
            void const* userobj,
            void (*payload)(send_archive_type*, void const*));

   private:
    std::unique_ptr<detail::basic_dispatcher_impl> self;
    int64_t _fence = 0;
};

}  // namespace perfkit::terminal::net
