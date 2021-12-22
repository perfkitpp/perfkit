//
// Created by ki608 on 2021-11-21.
//
// TODO: improve performance by removing intermediate marshalling to nlohmann::json
#pragma once
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <nlohmann/json.hpp>
#include <perfkit/common/algorithm.hxx>
#include <perfkit/common/algorithm/base64.hxx>
#include <perfkit/common/assert.hxx>
#include <perfkit/common/dynamic_array.hxx>
#include <perfkit/common/event.hxx>
#include <perfkit/common/hasher.hxx>
#include <perfkit/common/helper/nlohmann_json_macros.hxx>
#include <perfkit/common/memory/pool.hxx>
#include <perfkit/common/timer.hxx>
#include <perfkit/extension/net.hpp>
#include <perfkit/logging.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

namespace perfkit::terminal::net::detail {
using namespace std::literals;

using asio::ip::tcp;
using socket_id_t = perfkit::basic_key<class LABEL_socket_id_t>;

using std::chrono::steady_clock;
using timestamp_t       = std::chrono::steady_clock::time_point;
using recv_archive_type = nlohmann::json;
using send_archive_type = nlohmann::json;

struct basic_dispatcher_impl_init_info
{
    size_t buffer_size_hint   = 65535;
    size_t message_size_limit = 1 << 20;
    std::vector<auth_info> auth;
};

#pragma pack(push, 4)
struct message_header_t
{
    char identifier[4] = {'o', '`', 'P', '%'};
    char buflen[4]     = {};
};
static_assert(sizeof(message_header_t) == 8);
#pragma pack(pop)

struct message_in_t
{
    std::string route;
    recv_archive_type parameter;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(message_in_t, route, parameter);
};

struct message_out_t
{
    std::string route;
    uint64_t fence;
    send_archive_type payload;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(message_out_t, route, fence, payload);
};

struct connection_context_t
{
    std::unique_ptr<tcp::socket> socket;
    std::vector<char> rdbuf;
    bool is_active = false;

    timestamp_t recv_latest;
};

class basic_dispatcher_impl
{
   public:
    using init_info = basic_dispatcher_impl_init_info;
    using buffer_t  = std::vector<char>;

   public:
    virtual ~basic_dispatcher_impl()
    {
        shutdown();
    };

    explicit basic_dispatcher_impl(init_info s)
            : _cfg(std::move(s))
    {
        // TODO: configure auth
        //         1. generate tokens from id, pw
        //         2. clear pw of auth_info
        //         3. emplace auth_info to auth, using token as key.
        //         4. clear _cfg.auth vector
    }

   public:
    void launch()
    {
        _alive     = true;
        _io_worker = std::thread{
                [&] {
                    while (_alive)
                    {
                        try
                        {
                            _io.restart();
                            refresh();

                            CPPH_INFO("running io context ...");
                            _io.run(), CPPH_INFO("IO CONTEXT STOPPED");
                        }
                        catch (asio::system_error& e)
                        {
                            CPPH_ERROR("uncaught asio exception: {} (what(): {})", e.code().message(), e.what());
                            CPPH_INFO("retrying refresh after 3 seconds ...");
                            std::this_thread::sleep_for(3s);
                        }
                    }

                    cleanup();
                }};
    }

    void shutdown()
    {
        if (not _alive)
            return;

        _alive.store(false);
        if (_io_worker.joinable())
        {
            CPPH_INFO("shutting down async worker thread for {} connections ...", _connections.size());

            _io.stop();
            _io_worker.join();

            _connections.clear();
            _sockets_active.clear();

            CPPH_INFO("done. all connections disposed.");
        }
    }

    void register_recv(
            std::string route,
            std::function<bool(recv_archive_type const& parameter)> handler)
    {
        auto lc{std::lock_guard{_mtx_modify}};
        auto [it, is_new] = _recv_routes.try_emplace(std::move(route), std::move(handler));

        assert_(is_new && "duplicated route is logic error ! ! !");
    }

    void send(
            std::string_view route,
            int64_t fence,
            void const* userobj,
            void (*payload)(send_archive_type*, void const*))
    {
        if (false && _disconnect_timer.check())
        {  // iterate each socket, and disconnect obsolete ones.
            std::forward_list<socket_id_t> expired;

            lock_guard{_mtx_modify}, [&] {
                for (auto& [key, ctx] : _connections)
                    if (steady_clock::now() - ctx.recv_latest > 5s)
                        expired.push_front(key);
            }();

            for (auto key : expired)
                close(key, "no-activity");
        }

        // iterate all active sockets and push payload
        send_archive_type archive;
        archive["route"] = route;
        archive["fence"] = fence;
        payload(&archive["payload"], userobj);

        auto lc{std::lock_guard{_mtx_modify}};

        auto buffer = _send_pool.checkout();
        buffer->clear();
        buffer->resize(8);
        nlohmann::json::to_msgpack(archive, {*buffer});

        (*buffer)[0]         = 'o';
        (*buffer)[1]         = '`';
        (*buffer)[2]         = 'P';
        (*buffer)[3]         = '%';
        *(int*)&(*buffer)[4] = buffer->size() - 8;

        for (auto sock : _sockets_active)
        {
            auto pbuf = &*buffer;
            asio::async_write(
                    *sock, asio::const_buffer{pbuf->data(), pbuf->size()},
                    [this, buffer = std::move(buffer)](auto&&, auto&& n) { _perf_out(n); });
        }
    }

    size_t bytes_out() const noexcept
    {
        return _perf_bytes_out;
    }

    size_t bytes_in() const noexcept
    {
        return _perf_bytes_in;
    }

    perfkit::event<int> on_new_connection;
    perfkit::event<> on_no_connection;

    auto* io() { return &_io; }

   protected:
    // 워커 스레드에서, io_context가 초기화될 때마다 호출.
    // 구현에 따라, 서버 소켓을 바인딩하거나, 서버에 연결하는 등의 용도로 사용.
    virtual void refresh() {}

    // shutdown() 시 호출. 서버 accept 소켓 등을 정리하는 데 사용.
    virtual void cleanup() {}

   protected:  // deriving classes may call this
    // 구현부에서 호출. 새로운 client endpoint 생성 시 호출. 서버 accept 소켓 등은 해당 안 됨!
    void notify_new_connection(socket_id_t id, std::unique_ptr<tcp::socket> socket)
    {
        std::lock_guard lck{_mtx_modify};
        CPPH_INFO("new connection notified: {}", id.value);

        auto [it, is_new] = _connections.try_emplace(id);
        assert_(is_new);  // id must be unique!
        auto* context        = &it->second;
        context->socket      = std::move(socket);
        context->recv_latest = steady_clock::now();

        auto* sock = &*context->socket;
        asio::async_read(
                *sock, asio::dynamic_buffer(context->rdbuf).prepare(8),
                asio::transfer_all(),
                [this, id, context](auto&& a, auto&& b) {
                    _handle_prelogin_header(id, context, a, b);
                });
    }

    void close(socket_id_t id, std::string_view why)
    {
        CPPH_INFO("disconnecting socket {} for '{}'", id.value, why);

        bool zero_connection = false;

        {
            std::lock_guard lc{_mtx_modify};
            auto it = _connections.find(id);

            if (it == _connections.end())
            {
                CPPH_WARN("socket {} already destroyed.", id.value);
                return;
            }

            auto* socket = &*it->second.socket;

            socket->close();
            auto it_active = perfkit::find(_sockets_active, socket);
            if (it_active != _sockets_active.end())
                _sockets_active.erase(it_active);
            _connections.erase(it);

            CPPH_INFO("--> socket {} disconnected.", id.value);

            zero_connection = _sockets_active.empty();
        }

        if (zero_connection)
            on_no_connection.invoke();
    }

   private:  // handler helpers
    static auto _buf(buffer_t& t, size_t n)
    {
        t.resize(n);
        return asio::buffer(t);
    }

    static auto _view(buffer_t& t, size_t n)
    {
        return array_view{t.data(), n};
    }

    static auto _strview(buffer_t& t, size_t n)
    {
        return std::string_view{t.data(), n};
    }

   private:  // handlers
    size_t _handle_header(
            socket_id_t id, connection_context_t* ctx,
            asio::error_code const& ec, size_t n_read)
    {
        if (ec || n_read != 8)
        {
            CPPH_WARN("failed to receive header from socket {}: {}", id.value, ec.message());
            close(id, "invalid-header");
            return ~size_t{};
        }

        _perf_in(n_read);

        auto size = _retrieve_buffer_size(_view(ctx->rdbuf, n_read).as<message_header_t>());
        if (size == ~size_t{})
        {
            CPPH_ERROR("socket {}: protocol error, closing connection ...", id.value);
            close(id, "invalid-buffer-size");
            return ~size_t{};
        }

        ctx->recv_latest = steady_clock::now();
        return size;
    }

    void _handle_prelogin_header(
            socket_id_t id, connection_context_t* context,
            asio::error_code const& ec, size_t n_read)
    {
        auto size = _handle_header(id, context, ec, n_read);
        if (~size_t{} == size)
            return;

        asio::async_read(
                *context->socket, _buf(context->rdbuf, size),
                asio::transfer_all(),
                [this, id, context](auto&& a, auto&& b) {
                    _handle_prelogin_body(id, context, a, b);
                });
    }

    void _handle_prelogin_body(
            socket_id_t id, connection_context_t* ctx,
            asio::error_code const& ec, size_t n_read)
    {
        if (ec)
        {
            CPPH_WARN("failed to receive header from socket {}", id.value);
            close(id, "disconnected");
            return;
        }
        _perf_in(n_read);

        bool login_success       = false;
        bool access_readonly     = true;
        std::string_view auth_id = "<none>";

        if (_auth.empty())
        {
            // there's no authentication info.
            login_success   = true;
            access_readonly = false;
            CPPH_WARN(
                    "no authentication information registered."
                    " always passing login attempt...");
        }
        else
        {
            message_in_t message;
            try
            {
                auto buf = _view(ctx->rdbuf, n_read);
                message  = nlohmann::json::from_msgpack(buf.begin(), buf.end());

                if (message.route != "auth:login")
                    throw std::exception{};

                auto& id_str = message.parameter.at("id").get_ref<std::string&>();
                auto& auth   = _auth.at(id_str);

                CPPH_INFO("login attempt to id {}");

                if (auth.password != message.parameter.at("pw").get_ref<std::string&>())
                    throw std::runtime_error("password mismatch");

                auth_id         = auth.id;
                login_success   = true;
                access_readonly = auth.readonly_access;
            }
            catch (std::exception& e)
            {
                CPPH_ERROR("failed to parse login message: {}", _strview(ctx->rdbuf, n_read));
            }
        }

        if (not login_success)
        {
            CPPH_WARN("invalid login attempt from {}",
                      ctx->socket->remote_endpoint().address().to_string());

            asio::async_read(
                    *ctx->socket, _buf(ctx->rdbuf, 8),
                    asio::transfer_all(),
                    [this, id, ctx](auto&& a, auto&& b) {
                        _handle_prelogin_header(id, ctx, a, b);
                    });
            return;
        }
        else
        {
            auto ep = ctx->socket->remote_endpoint();
            CPPH_INFO("${}@{}:{}. login as {} successful",
                      auth_id, ep.address().to_string(), ep.port(),
                      access_readonly ? "readonly-session" : "admin-session");
        }

        int n_connections = 0;
        {
            auto lc{std::lock_guard{_mtx_modify}};
            _sockets_active.push_back(&*ctx->socket);
            n_connections  = _sockets_active.size();
            ctx->is_active = true;

            if (access_readonly)
                ctx->socket->async_read_some(
                        _buf(ctx->rdbuf, _cfg.buffer_size_hint),
                        [this, id, ctx](auto&& a, auto&& b) {
                            _handle_discarded(id, ctx, a, b);
                        });
            else
                asio::async_read(
                        *ctx->socket, _buf(ctx->rdbuf, 8),
                        asio::transfer_all(),
                        [this, id, ctx](auto&& a, auto&& b) {
                            _handle_active_header(id, ctx, a, b);
                        });
        }

        on_new_connection.invoke(n_connections);
    }

    void _handle_discarded(
            socket_id_t id, connection_context_t* ctx,
            asio::error_code const& ec, size_t n_read)
    {
        if (ec)
            return;

        ctx->recv_latest = steady_clock::now();

        _perf_in(n_read);
        ctx->socket->async_read_some(
                _buf(ctx->rdbuf, _cfg.buffer_size_hint),
                [this, id, ctx](auto&& a, auto&& b) {
                    _handle_discarded(id, ctx, a, b);
                });
    }

    void _handle_active_header(
            socket_id_t id, connection_context_t* ctx,
            asio::error_code const& ec, size_t n_read)
    {
        auto size = _handle_header(id, ctx, ec, n_read);
        if (~size_t{} == size)
            return;

        asio::async_read(
                *ctx->socket, _buf(ctx->rdbuf, size),
                asio::transfer_all(),
                [this, id, ctx](auto&& a, auto&& b) {
                    _handle_active_body(id, ctx, a, b);
                });
    }

    void _handle_active_body(
            socket_id_t id, connection_context_t* ctx,
            asio::error_code const& ec, size_t n_read)
    {
        if (ec)
        {
            CPPH_WARN("failed to receive header from socket {}", id.value);
            return;
        }
        _perf_in(n_read);

        message_in_t message;

        try
        {
            auto buf = _view(ctx->rdbuf, n_read);
            message  = nlohmann::json::from_msgpack(buf.begin(), buf.begin() + n_read);

            if (message.route == "cmd:heartbeat")
            {
                ;  // silently ignore message.
            }
            else
            {
                decltype(_recv_routes)::mapped_type* fn;
                {
                    std::lock_guard lc{_mtx_modify};
                    fn = &_recv_routes.at(message.route);
                }

                (*fn)(message.parameter);
            }
        }
        catch (std::out_of_range& e)
        {
            CPPH_CRITICAL("INVALID ROUTE RECEIVED: {}", message.route);
        }
        catch (std::exception& e)
        {
            CPPH_ERROR("failed to parse input messagepack: {}", e.what());
        }

        asio::async_read(
                *ctx->socket, _buf(ctx->rdbuf, 8),
                asio::transfer_all(),
                [this, id, ctx](auto&& a, auto&& b) {
                    _handle_active_header(id, ctx, a, b);
                });
    }

    void _perf_in(size_t n)
    {
        _perf_bytes_in += n;
    }

    void _perf_out(size_t n)
    {
        _perf_bytes_out += n;
    }

    size_t _retrieve_buffer_size(message_header_t h)
    {
        static constexpr message_header_t reference;

        // check if header size does not exceed buffer length
        size_t decoded = 0;
        decoded        = *(int*)h.buflen;

        if (decoded >= _cfg.message_size_limit)
            return CPPH_WARN("buffer size exceeds max: {} [max {}B]",
                             decoded, _cfg.message_size_limit),
                   ~size_t{};

        return decoded;
    }

   protected:
    spdlog::logger* CPPH_LOGGER() const { return _logger.get(); }

   private:
    init_info _cfg;
    std::map<std::string, auth_info> _auth;

    asio::io_context _io{1};
    std::unordered_map<socket_id_t, connection_context_t> _connections;
    std::vector<tcp::socket*> _sockets_active;

    std::atomic_bool _alive{false};
    std::thread _io_worker;

    pool<std::string> _send_pool;
    poll_timer _perf_timer{1s};
    poll_timer _disconnect_timer{3s};
    size_t _perf_bytes_out = 0;
    size_t _perf_bytes_in  = 0;

    std::string _token;      // login token to compare with.
    std::mutex _mtx_modify;  // locks when modification (socket add/remove) occurs.

    std::map<std::string, std::function<bool(recv_archive_type const& parameter)>>
            _recv_routes;

    logger_ptr _logger = share_logger("PERFKIT:NET");
};
}  // namespace perfkit::terminal::net::detail