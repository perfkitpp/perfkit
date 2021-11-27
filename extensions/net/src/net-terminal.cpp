#include "net-terminal.hpp"

perfkit::terminal::net::terminal::terminal(
        const perfkit::terminal::net::terminal::init_info& init)
        : _io(init)
{
    // redirect stdout
    if (init.advanced.redirect_terminal)
        detail::input_redirect(CPPH_BIND(_char_handler));

    // launch user command fetcher
    _worker_user_command = std::thread(CPPH_BIND(_user_command_fetch_fn));
    _worker              = std::thread{CPPH_BIND(_exec)};

    // register handlers
    _io.on_recv<incoming::push_command>("cmd:push_command", CPPH_BIND(_on_push_command));

    // launch asynchronous IO thread.
    _io.launch();
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

void perfkit::terminal::net::terminal::_on_no_connection()
{
    if (_any_connection.exchange(false))
    {
        CPPH_INFO("last connection has been disconnected. rolling back redirected stdout...");

        _touch_worker();
    }
}

void perfkit::terminal::net::terminal::_on_any_connection(int n_conn)
{
    if (not _any_connection.exchange(true) && n_conn >= 1)
    {
        CPPH_INFO("{} connections initially established. redirecting stdout ...", n_conn);

        _any_connection.store(true);
    }

    _new_connection_exist = true;
    _touch_worker();
}

void perfkit::terminal::net::terminal::_user_command_fetch_fn()
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

void perfkit::terminal::net::terminal::_on_push_command(perfkit::terminal::net::incoming::push_command&& s)
{
    push_command(s.command);
}

void perfkit::terminal::net::terminal::_exec()
{
    auto is_dirty = [&]
    {
        return _dirty;
    };

    // initial state binding
    _worker_state = CPPH_BIND(_worker_idle);

    CPPH_INFO("entering worker loop...");
    while (_active)
    {
        // invoke state loop
        _worker_state();

        // wait for any event
        std::unique_lock lc{_mtx_worker};
        _cvar_worker.wait(lc, is_dirty);
        _dirty = false;
    }
}

void perfkit::terminal::net::terminal::_worker_idle()
{
    if (_any_connection)
    {
        _transition(CPPH_BIND(_worker_boostrap));
    }
    else
    {
        ;  // otherwise, do nothing.
    }
}

void perfkit::terminal::net::terminal::_worker_boostrap()
{
    _new_connection_exist = false;
    _context              = {};

    // send session information

    //

    _transition(CPPH_BIND(_worker_exec));
}

void perfkit::terminal::net::terminal::_transition(std::function<void()> fn)
{
    _worker_state = std::move(fn);
    _dirty        = true;
}

void perfkit::terminal::net::terminal::_worker_exec()
{
    if (not _any_connection)
    {
        _transition(CPPH_BIND(_worker_idle));
        return;
    }

    if (_new_connection_exist)
    {
        _transition(CPPH_BIND(_worker_boostrap));
        return;
    }
}
