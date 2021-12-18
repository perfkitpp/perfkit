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
        auto cmd = term->fetch_command(1s);
        if (not cmd || cmd->empty())
            continue;

        spdlog::info("command: [{}]", *cmd);
        term->commands()->invoke_command(*cmd);
    }
}
