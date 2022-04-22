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
// Created by Seungwoo on 2021-09-25.
//

#pragma once
#include <future>
#include <list>

#include "cpph/circular_queue.hxx"
#include "perfkit/detail/commands.hpp"
#include "perfkit/terminal.h"

namespace perfkit {
class basic_interactive_terminal : public if_terminal
{
   public:
    basic_interactive_terminal();

   public:
    std::optional<std::string> fetch_command(milliseconds timeout) override;
    void write(std::string_view str) override;
    void push_command(std::string_view command) override;

   private:
    void _register_autocomplete();
    void _unregister_autocomplete();

   private:
    std::shared_ptr<spdlog::sinks::sink> _sink;

    std::future<std::string> _cmd;
    circular_queue<std::string> _cmd_history{16};
    size_t _cmd_counter = 0;

    std::mutex _cmd_queue_lock;
    std::list<std::string> _cmd_queued;

    std::string _prompt = "$ ";
};
}  // namespace perfkit
