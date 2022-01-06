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

#include <perfkit/detail/commands.hpp>
#include <perfkit/extension/net.hpp>
#include <perfkit/terminal.h>
#include <spdlog/spdlog.h>

#include "test_class.hpp"

using namespace std::literals;

PERFKIT_CATEGORY(conf_global)
{
    PERFKIT_CONFIGURE(some_selectible, "hello")
            .one_of({"hello", "my name", "is john"})
            .confirm();
}

int main(void)
{
    spdlog::set_level(spdlog::level::debug);

    test_class test1{"test1"};
    test_class test2{"test2"};
    test_class test3{"test3"};

    test1.start();
    test2.start();
    test3.start();

    auto term = perfkit::terminal::net::create("__terminal");
    perfkit::terminal::initialize_with_basic_commands(&*term);

    bool running = true;
    term->add_command(
            "quit",
            [&] {
                running = false;
            });

    while (running)
    {
        conf_global::update();

        auto cmd = term->fetch_command(1s);
        if (not cmd || cmd->empty())
            continue;

        spdlog::info("command: [{}]", *cmd);
        term->commands()->invoke_command(*cmd);
    }
}
