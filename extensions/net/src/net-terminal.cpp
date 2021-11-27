#include "net-terminal.hpp"

perfkit::terminal::net::terminal::terminal(
        const perfkit::terminal::net::terminal::init_info& init)
        : _io(init)
{
    // launch user command fetcher
    _user_command_fetcher = std::thread(CPPH_BIND(_user_command_fetch_fn));

    // register number of handlers

    // redirect stdout, stderr
    detail::input_redirect(CPPH_BIND(_char_handler));
}

void perfkit::terminal::net::terminal::_char_handler(char c)
{
    _shell_buffered.content += c;

    if (c == '\n')
    {
        // send buffer as message
        _io.send("update:shell_output", _shell_buffered);
        _shell_buffered.content.clear();
    }
}
