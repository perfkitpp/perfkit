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

#include <cpph/refl/object.hxx>
#include <cpph/refl/types/tuple.hxx>
#include <perfkit/configs-v2.h>
#include <perfkit/detail/commands.hpp>
#include <perfkit/extension/net.hpp>
#include <perfkit/logging.h>
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

PERFKIT_GCFG_CAT(MyGCAT::MySUBCAT)
{
    PERFKIT_GCFG(MyGc, 3.41);
}

PERFKIT_GCFG_CAT_ROOT_def(MyGCAT::MySUBCAT) {}

#define DefaultValue 3

PERFKIT_CFG_CLASS(MyCfg)
{
    PERFKIT_CFG(MyInt, 1, "Raw");
    PERFKIT_CFG(MyString, "string");
    PERFKIT_CFG(Vodif, std::vector<std::pair<int, double>>{}, "string");
};

PERFKIT_CFG_CLASS(OtherCfg)
{
    PERFKIT_CFG_SUBSET(MyCfg, Cfg1);
};

PERFKIT_CFG_CLASS(OtherCfg2)
{
    PERFKIT_CFG_SUBSET(OtherCfg, Cfg0);
};

int main(int argc, char** argv)
{
    if (argc > 1)
        perfkit::configs_import(argv[1]);

    auto f = OtherCfg2::create("hola!");
    f->update();

    spdlog::set_level(spdlog::level::debug);

    test_class test1{"test1"};
    test_class test2{"test2"};
    test_class test3{"test3"};

    test1.start();
    test2.start();
    test3.start();

    auto logging = perfkit::create_log_monitor();
    auto profile = perfkit::terminal::net::profile::create("__NET_TERM");
    profile.bind_port.set(15572);

    auto term = perfkit::terminal::net::create(profile);
    perfkit::terminal::initialize_with_basic_commands(&*term);
    perfkit::terminal::register_conffile_io_commands(&*term);

    volatile bool running = true;
    term->add_command("quit", [&] { running = false; });
    conf_global::update();
    while (running) {
        term->invoke_queued_commands(
                500ms,
                [&] { return running; },
                [&](auto cmd) {
                    conf_global::update();
                    spdlog::info("cmd: {}", cmd);
                });
        tick_log_monitor(*logging);
    }

    spdlog::info("Now shutting down ...");

    if (argc > 1)
        perfkit::configs_export(argv[1]);
    return 0;
}
