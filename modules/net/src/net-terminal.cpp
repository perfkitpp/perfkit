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

#include "net-terminal.hpp"

#include "cpph/algorithm/std.hxx"

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
        _init_msg.name = init.name;
        _init_msg.description = init.description;
        _init_msg.num_cores = std::thread::hardware_concurrency();

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

    // set command queue capacity
    // some kind of applications may not fetch command.
    _command_queue.set_capacity(128);

    // redirect stdout
    if (init.advanced.redirect_terminal)
        detail::input_redirect(CPPH_BIND(_char_handler));

    // launch user command fetcher
    _worker_user_command = std::thread(CPPH_BIND(_user_command_fetch_fn));
    _worker = std::thread{CPPH_BIND(_exec)};

    // register events
    _io.on_new_connection() += CPPH_BIND(_on_any_connection);
    _io.on_no_connection() += CPPH_BIND(_on_no_connection);

    // register handlers
    _io.on_recv<incoming::push_command>(CPPH_BIND(_on_push_command));
    _io.on_recv<incoming::configure_entity>(CPPH_BIND(_on_configure));
    _io.on_recv<incoming::suggest_command>(CPPH_BIND(_on_suggest_request));
    _io.on_recv<incoming::control_trace>(CPPH_BIND(_on_trace_tweak));
    _io.on_recv<incoming::signal_fetch_traces>(CPPH_BIND(_on_trace_signal));

    // launch asynchronous IO thread.
    _io.launch();

    // add command for dropping all connections
    commands()->root()->add_subcommand("term-drop-all-connection", [this] { _io.close_all(); });
}

void perfkit::terminal::net::terminal::_char_handler(char c)
{
    if (not _shell_active.test_and_set()) {
        _shell_buffered.content = _shell_accumulated;
    }

    if (_any_connection) {
        _shell_buffered.content += c;

        if (c == '\n' || (c == '\r' && _shell_buffered.content.size() > 1)) {
            // send buffer as message
            _io.send(_shell_buffered);
            _shell_buffered.content.clear();
        }
    }

    _shell_accumulated += c;

    int constexpr NUM_MAX_BUF = 1 << 16;  // TODO: Resolve parsing error on too long buffer size
    if (_shell_accumulated.size() > NUM_MAX_BUF) {
        _shell_accumulated.erase(
                _shell_accumulated.begin(),
                _shell_accumulated.end() - NUM_MAX_BUF / 2);
    }
}

void perfkit::terminal::net::terminal::_on_no_connection()
{
    if (_any_connection.exchange(false)) {
        _touch_worker();
    }
}

void perfkit::terminal::net::terminal::_on_any_connection(int n_conn)
{
    if (not _any_connection.exchange(true) && n_conn >= 1) {
        CPPH_INFO("{} connections initially established. redirecting stdout ...", n_conn);

        _any_connection.store(true);
    }

    _new_connection_exist = true;
    _touch_worker();
}

void perfkit::terminal::net::terminal::_user_command_fetch_fn()
{
    perfkit::poll_timer tmr{500ms};
    detail::proc_stat_t stat = {};

    while (_active) {
        auto wait = std::chrono::duration_cast<decltype(1ms)>(tmr.remaining());
        auto str = detail::try_fetch_input(wait.count());

        if (not str.empty()) {
            _command_queue.emplace(std::move(str));
        }

        if (tmr.check() && fetch_proc_stat(&stat)) {
            auto [in, out] = _io.num_bytes_in_out();
            auto delta_in = in - _bytes_io_prev.first;
            auto delta_out = out - _bytes_io_prev.second;
            auto dt = tmr.delta().count();
            int in_rate = delta_in / dt;
            int out_rate = delta_out / dt;
            _bytes_io_prev.first = in;
            _bytes_io_prev.second = out;

            outgoing::session_state message
                    = {stat.cpu_usage_total_user,
                       stat.cpu_usage_total_system,
                       stat.cpu_usage_self_user,
                       stat.cpu_usage_self_system,
                       stat.memory_usage_virtual,
                       stat.memory_usage_resident,
                       stat.num_threads,
                       out_rate, in_rate};

            _io.send(message);
        }
    }
}

void perfkit::terminal::net::terminal::_on_push_command(
        perfkit::terminal::net::incoming::push_command&& s)
{
    push_command(s.command);
}

void perfkit::terminal::net::terminal::_on_configure(
        perfkit::terminal::net::incoming::configure_entity&& s)
{
    for (auto& entity : s.content) {
        _context.configs.update_entity(
                entity.config_key,
                std::move(entity.value));
    }
}

void perfkit::terminal::net::terminal::_on_suggest_request(
        perfkit::terminal::net::incoming::suggest_command&& s)
{
    std::vector<std::string> candidates;
    auto nextstr = commands()->suggest(s.command, &candidates);

    outgoing::suggest_command cmd;
    cmd.new_command = std::move(nextstr);
    cmd.reply_to = s.reply_to;
    perfkit::transform(
            candidates,
            std::front_inserter(cmd.candidates),
            [](auto& s) { return std::move(s); });

    _io.send(cmd);
}

void perfkit::terminal::net::terminal::_on_trace_signal(
        perfkit::terminal::net::incoming::signal_fetch_traces&& s)
{
    for (auto& sig : s.targets) {
        _context.traces.signal(sig);
    }
}

void perfkit::terminal::net::terminal::_on_trace_tweak(incoming::control_trace&& s)
{
    _context.traces.tweak(
            s.trace_key,
            s.subscribe ? &*s.subscribe : nullptr,
            s.fold ? &*s.fold : nullptr);
}

void perfkit::terminal::net::terminal::_exec()
{
    // initial state binding
    _worker_state = CPPH_BIND(_worker_idle);

    CPPH_INFO("entering worker loop...");
    while (_active) {
        // invoke state loop
        _worker_state();

        // wait for any event
        std::unique_lock lc{_mtx_worker};
        _cvar_worker.wait_for(lc, 3s, [&] { return _dirty; });
        _dirty = false;
    }

    CPPH_INFO("stopping watchers ...");
    context::if_watcher* watchers[]
            = {
                    &_context.configs,
                    &_context.traces,
                    &_context.graphics,
            };

    for (auto* watcher : watchers) {
        watcher->stop();
    }
    CPPH_INFO("exiting worker thread ...");
}

void perfkit::terminal::net::terminal::_worker_idle()
{
    if (_any_connection) {
        CPPH_INFO("new connection found. transitioning to bootstrap state...");
        _transition(CPPH_BIND(_worker_boostrap));
    } else {
        ;  // otherwise, do nothing.
    }
}

void perfkit::terminal::net::terminal::_worker_boostrap()
{
    _new_connection_exist = false;

    // register contexts
    context::if_watcher* watchers[] = {
            &_context.configs, &_context.traces, &_context.graphics};

    for (auto* watcher : watchers) {
        watcher->stop();
        watcher->notify_change = CPPH_BIND(_touch_worker);
        watcher->io = &_io;
        watcher->start();
    }

    // send session information
    _io.send(_init_msg);
    _shell_active.clear();

    CPPH_INFO("bootstrapping finished. transitioning to execution state ...");
    _transition(CPPH_BIND(_worker_exec));
}

void perfkit::terminal::net::terminal::_transition(std::function<void()> fn)
{
    _worker_state = std::move(fn);
    _dirty = true;
}

void perfkit::terminal::net::terminal::_worker_exec()
{
    if (not _any_connection) {
        CPPH_INFO("last connection has been lost. returning to idling state ...");
        _transition(CPPH_BIND(_worker_idle));
        return;
    }

    if (_new_connection_exist) {
        CPPH_INFO("new connection found. resetting all state ...");
        _transition(CPPH_BIND(_worker_boostrap));
        return;
    }

    context::if_watcher* watchers[] = {
            &_context.configs, &_context.traces, &_context.graphics};

    for (auto* watcher : watchers) {
        watcher->update();
    }
}
