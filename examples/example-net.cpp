#include <perfkit/detail/commands.hpp>
#include <perfkit/extension/net.hpp>
#include <perfkit/terminal.h>
#include <spdlog/spdlog.h>

#include "test_class.hpp"

//
#include "test_configs.inl.h"

using namespace std::literals;

int main(void)
{
    test_class test1{"test1"};
    test_class test2{"test2"};
    test_class test3{"test3"};

    test1.start();
    test2.start();
    test3.start();

    perfkit::terminal::net::terminal_init_info init{"example-net"};
    init.serve(25572);
    bool running = true;

    auto term = create(init);
    perfkit::terminal::initialize_with_basic_commands(&*term);
    term->add_command(
            "quit",
            [&]
            {
                running = false;
            });

    while (running)
    {
        auto cmd = term->fetch_command(1000s);
        if (not cmd || cmd->empty())
            continue;

        spdlog::info("command: {}", *cmd);
        term->commands()->invoke_command(*cmd);
    }
}
