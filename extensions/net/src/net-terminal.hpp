#pragma once
#include <future>
#include <iostream>

#include <perfkit/common/assert.hxx>
#include <perfkit/common/thread/notify_queue.hxx>
#include <perfkit/detail/commands.hpp>
#include <perfkit/extension/net.hpp>
#include <perfkit/terminal.h>
#include <spdlog/spdlog.h>

#include "dispatcher.hpp"
#include "utils.hpp"

namespace perfkit::terminal::net
{
using namespace std::literals;

class terminal : public if_terminal
{
    using init_info = terminal_init_info;

   public:
    explicit terminal(init_info const& init) : _io(init)
    {
        // register number of handlers
    }

    commands::registry* commands() override
    {
        return &_commands;
    }

    std::optional<std::string> fetch_command(milliseconds timeout) override
    {
        if (_user_command_fetcher.valid()
            && _user_command_fetcher.wait_for(0ms) == std::future_status::ready)
        {
            _user_command_fetcher.get();
        }

        if (not _user_command_fetcher.valid())
        {
            _user_command_fetcher = std::async(CPPH_BIND(_fetch_worker), timeout.count());
        }

        return _command_queue.try_pop(timeout);
    }

    void push_command(std::string_view command) override
    {
        _command_queue.emplace(command);
    }

    void write(std::string_view str, termcolor fg, termcolor bg) override
    {
        std::cout << str;
    }

    std::shared_ptr<spdlog::sinks::sink> sink() override
    {
        return spdlog::default_logger_raw()->sinks().front();
    }

   private:
    void _fetch_worker(int ms_to_wait)
    {
        auto str = detail::try_fetch_input(ms_to_wait);

        if (not str.empty())
        {
            _command_queue.emplace(std::move(str));
        }
    }

   private:
    dispatcher _io;
    commands::registry _commands;

    std::future<void> _user_command_fetcher;
    notify_queue<std::string> _command_queue;
};

}  // namespace perfkit::terminal::net