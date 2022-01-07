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
// Created by ki608 on 2021-11-27.
//

#pragma once

#include "if_watcher.hpp"
#include "perfkit/common/hasher.hxx"
#include "perfkit/common/template_utils.hxx"
#include "perfkit/common/thread/notify_queue.hxx"
#include "perfkit/common/thread/worker.hxx"
#include "perfkit/common/timer.hxx"
#include "perfkit/detail/configs.hpp"
#include "perfkit/extension/net-internals/messages.hpp"

namespace asio {
class io_context;
}

namespace perfkit::terminal::net::context {
using namespace std::literals;

using config_key_t = basic_key<class LABEL_config_key_t>;

class config_watcher : public if_watcher
{
   public:
    void start() override;
    void stop() override;
    void update() override;

    void update_entity(uint64_t key, nlohmann::json&& value);

   public:
    config_watcher();
    ~config_watcher() override;

   private:
    void _publish_registry(config_registry* rg);

   private:
    thread::worker _worker;

    std::unique_ptr<asio::io_context> _ioc;
    perfkit::shared_null _watcher_lifecycle;

    struct _cache_type
    {
        std::vector<std::weak_ptr<perfkit::config_registry>> regs;
        std::unordered_map<config_key_t, perfkit::config_wptr> confmap;
    } _cache;
};

}  // namespace perfkit::terminal::net::context
