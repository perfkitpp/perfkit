#include "net-terminal.hpp"

extern "C" int gethostname(char*, size_t);

perfkit::terminal::net::terminal::terminal(
        const perfkit::terminal::net::terminal::init_info& init)
        : _io(init)
{
    {
        using namespace std::chrono;
        _init_msg.epoch = duration_cast<milliseconds>(
                                  steady_clock::now().time_since_epoch())
                                  .count();
        _init_msg.name        = init.name;
        _init_msg.description = init.description;
        _init_msg.num_cores   = std::thread::hardware_concurrency();

        // key string generation
        char hostname[65];
        gethostname(hostname, std::size(hostname));

        _init_msg.hostname = hostname;
        _init_msg.keystr
                = _init_msg.hostname + ';'
                + std::to_string(_init_msg.num_cores) + ';'
                + _init_msg.name + ';'
                + std::to_string(_init_msg.epoch);
    }

    // redirect stdout
    if (init.advanced.redirect_terminal)
        detail::input_redirect(CPPH_BIND(_char_handler));

    // launch user command fetcher
    _worker_user_command = std::thread(CPPH_BIND(_user_command_fetch_fn));
    _worker              = std::thread{CPPH_BIND(_exec)};

    // register events
    _io.on_new_connection() += CPPH_BIND(_on_any_connection);
    _io.on_no_connection() += CPPH_BIND(_on_no_connection);

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
        CPPH_INFO("new connection found. transitioning to bootstrap state...");
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

    // register contexts
    context::if_watcher* watchers[] = {
            &_context.configs, &_context.traces, &_context.graphics};

    for (auto* watcher : watchers)
    {
        watcher->stop();
        watcher->notify_change = CPPH_BIND(_touch_worker);
        watcher->io            = &_io;
        watcher->start();
    }

    // send session information
    _io.send("epoch", _init_msg);

    CPPH_INFO("bootstrapping finished. transitioning to execution state ...");
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
        CPPH_INFO("last connection has been lost. returning to idling state ...");
        _transition(CPPH_BIND(_worker_idle));
        return;
    }

    if (_new_connection_exist)
    {
        CPPH_INFO("new connection found. resetting all state ...");
        _transition(CPPH_BIND(_worker_boostrap));
        return;
    }

    context::if_watcher* watchers[] = {
            &_context.configs, &_context.traces, &_context.graphics};

    for (auto* watcher : watchers)
    {
        watcher->update();
    }
}
