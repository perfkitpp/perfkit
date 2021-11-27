#pragma once
#include <future>
#include <iostream>

#include <perfkit/common/assert.hxx>
#include <perfkit/common/thread/notify_queue.hxx>
#include <perfkit/detail/base.hpp>
#include <perfkit/detail/commands.hpp>
#include <perfkit/extension/net.hpp>
#include <perfkit/terminal.h>
#include <spdlog/spdlog.h>

#include "dispatcher.hpp"
#include "messages.hpp"
#include "utils.hpp"

namespace perfkit::terminal::net
{
using namespace std::literals;

class terminal : public if_terminal
{
    using init_info = terminal_init_info;

   public:
    explicit terminal(init_info const& init);

    ~terminal()
    {
        detail::input_rollback();

        _active.store(false);
        _user_command_fetcher.joinable() && (_user_command_fetcher.join(), 0);
    }

    commands::registry* commands() override
    {
        return &_commands;
    }

    std::optional<std::string> fetch_command(milliseconds timeout) override
    {
        return _command_queue.try_pop(timeout);
    }

    void push_command(std::string_view command) override
    {
        _command_queue.emplace(command);
    }

    void write(std::string_view str, termcolor fg, termcolor bg) override
    {
        fwrite(str.data(), str.size(), 1, stdout);
    }

    std::shared_ptr<spdlog::sinks::sink> sink() override
    {
        return spdlog::default_logger_raw()->sinks().front();
    }

   private:
    void _user_command_fetch_fn()
    {
        while (_active)
        {
            auto str = detail::try_fetch_input(500);

            if (not str.empty())
            {
                _command_queue.emplace(std::move(str));
            }
        }
    }

    void _char_handler(char c);

    void _on_push_command(incoming::push_command&& s)
    {
        push_command(s.command);
    }

   private:
    dispatcher _io;
    commands::registry _commands;

    std::atomic_bool _active = true;

    std::thread _user_command_fetcher;
    notify_queue<std::string> _command_queue;

    outgoing::shell_output _shell_buffered;
};

}  // namespace perfkit::terminal::net