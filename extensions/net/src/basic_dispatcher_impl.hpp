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
#include <perfkit/common/algorithm/base64.hxx>
#include <perfkit/common/assert.hxx>
#include <perfkit/common/dynamic_array.hxx>
#include <perfkit/common/hasher.hxx>
#include <perfkit/common/helper/nlohmann_json_macros.hxx>
#include <perfkit/common/timer.hxx>
#include <perfkit/detail/helpers.hpp>
#include <perfkit/extension/net.hpp>
#include <spdlog/spdlog.h>

namespace perfkit::terminal::net::detail
{
using namespace std::literals;

using asio::ip::tcp;
using socket_id_t = perfkit::basic_key<class LABEL_socket_id_t>;

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
    char identifier[4]    = {'o', '`', 'P', '%'};
    char base64_buflen[4] = {};
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

class basic_dispatcher_impl
{
   public:
    using init_info  = basic_dispatcher_impl_init_info;
    using buffer_ptr = std::shared_ptr<std::vector<char>>;

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
                [&]
                {
                    while (_alive)
                    {
                        try
                        {
                            _io.restart();
                            refresh();

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
            CPPH_INFO("shutting down async worker thread ...");

            _io.stop();
            _io_worker.join();

            _sockets.clear();
            _sockets_active.clear();

            CPPH_INFO("done. all connections disposed.");
        }
    }

    void register_recv(
            std::string route,
            std::function<bool(recv_archive_type const& parameter)> handler)
    {
        auto lc{std::lock_guard{_mtx_modify}};
        auto [it, is_new] = _recv_routes.try_emplace(route, std::move(handler));

        assert_(is_new);
    }

    void send(
            std::string_view route,
            int64_t fence,
            void* userobj,
            void (*payload)(send_archive_type*, void*))
    {
        // iterate all active sockets and push payload
        send_archive_type archive;
        archive["route"] = route;
        archive["fence"] = fence;
        payload(&archive["body"], userobj);

        while (_n_sending > 0)
            std::this_thread::yield();

        _bf_send.clear();
        nlohmann::json::to_msgpack(archive, {_bf_send});

        {
            auto lc{std::lock_guard{_mtx_modify}};
            _n_sending = _sockets_active.size();
            for (auto sock : _sockets_active)
                asio::async_write(
                        *sock, asio::const_buffer{_bf_send.data(), _bf_send.size()},
                        [this](auto&&, auto&& n)
                        { --_n_sending, _perf_out(n); });
        }
    }

    size_t out_rate() const noexcept
    {
        return _perf_out_rate;
    }

    size_t in_rate() const noexcept
    {
        return _perf_in_rate;
    }

   protected:
    // 워커 스레드에서, io_context가 초기화될 때마다 호출.
    // 구현에 따라, 서버 소켓을 바인딩하거나, 서버에 연결하는 등의 용도로 사용.
    virtual void refresh() {}

    // shutdown() 시 호출. 서버 accept 소켓 등을 정리하는 데 사용.
    virtual void cleanup() {}

   protected:  // deriving classes may call this
    auto* io() { return &_io; }

    // 구현부에서 호출. 새로운 client endpoint 생성 시 호출. 서버 accept 소켓 등은 해당 안 됨!
    void notify_new_connection(socket_id_t id, std::unique_ptr<tcp::socket> socket)
    {
        std::lock_guard lck{_mtx_modify};

        auto [it, is_new] = _sockets.try_emplace(id, std::move(socket));
        assert_(is_new);  // id must be unique!
        auto [it_b, is_new_b] = _buffers.try_emplace(id, std::make_shared<std::vector<char>>());
        it_b->second->reserve(_cfg.buffer_size_hint);

        auto* sock = &*it->second;
        asio::async_read(
                *sock, asio::dynamic_buffer(*it_b->second).prepare(8),
                [this, id, sock, buf = it_b->second](auto&& a, auto&& b)
                {
                    _handle_prelogin_header(id, sock, buf, a, b);
                });
    }

    void close(socket_id_t id)
    {
        std::unique_lock lc{_mtx_modify};
        auto it = _sockets.find(id);
        assert_(it != _sockets.end());

        it->second->close();
        _sockets_active.erase(
                std::find(_sockets_active.begin(), _sockets_active.end(), &*it->second));
        _buffers.erase(id);
        _sockets.erase(it);
    }

   private:  // handler helpers
    static auto _buf(buffer_ptr& t, size_t n)
    {
        return asio::dynamic_buffer(*t).prepare(n);
    }

    static auto _view(buffer_ptr& t, size_t n)
    {
        return array_view{t->data(), n};
    }

    static auto _strview(buffer_ptr& t, size_t n)
    {
        return std::string_view{t->data(), n};
    }

   private:  // handlers
    size_t _handle_header(
            socket_id_t id, tcp::socket* sock, buffer_ptr& bf,
            asio::error_code const& ec, size_t n_read)
    {
        if (ec || n_read != 8)
        {
            CPPH_WARN("failed to receive header from socket {}", id.value);
            close(id);
            return ~size_t{};
        }

        _perf_in(n_read);

        auto size = _retrieve_buffer_size(_view(bf, n_read).as<message_header_t>());
        if (size == ~size_t{})
        {
            CPPH_ERROR("socket {}: protocol error, closing connection ...");
            close(id);
            return ~size_t{};
        }

        return size;
    }

    void _handle_prelogin_header(
            socket_id_t id, tcp::socket* sock, buffer_ptr buf,
            asio::error_code const& ec, size_t n_read)
    {
        auto size = _handle_header(id, sock, buf, ec, n_read);
        if (~size_t{} == size)
            return;

        asio::async_read(
                *sock, _buf(buf, size),
                [this, id, sock, buf](auto&& a, auto&& b)
                {
                    _handle_prelogin_body(id, sock, buf, a, b);
                });
    }

    void _handle_prelogin_body(
            socket_id_t id, tcp::socket* sock, buffer_ptr bf,
            asio::error_code const& ec, size_t n_read)
    {
        if (ec)
        {
            CPPH_WARN("failed to receive header from socket {}", id.value);
            close(id);
            return;
        }
        _perf_in(n_read);

        bool login_success   = false;
        bool access_readonly = true;
        std::string_view auth_id;

        if (_auth.empty())
        {
            // there's no authentication info.
            login_success = true;
        }
        else
        {
            message_in_t message;
            try
            {
                auto buf = _view(bf, n_read);
                message  = nlohmann::json::parse(buf.begin(), buf.end());

                if (message.route != "auth:login")
                    throw std::exception{};

                auto& token = message.parameter.at("token").get_ref<std::string&>();
                auto& auth  = _auth.at(token);

                auth_id         = auth.id;
                login_success   = true;
                access_readonly = auth.readonly_access;
            }
            catch (std::exception& e)
            {
                CPPH_ERROR("failed to parse login message: {}", _strview(bf, n_read));
            }
        }

        if (not login_success)
        {
            CPPH_WARN("invalid login attempt from {}", sock->remote_endpoint().address().to_string());

            asio::async_read(
                    *sock, _buf(bf, 8),
                    [this, id, bf, sock](auto&& a, auto&& b)
                    {
                        _handle_prelogin_header(id, sock, bf, a, b);
                    });
            return;
        }
        else
        {
            auto ep = sock->remote_endpoint();
            CPPH_INFO("${}@{}:{}. login successful", auth_id, ep.address().to_string(), ep.port());
        }

        auto lc{std::lock_guard{_mtx_modify}};
        _sockets_active.push_back(sock);

        if (access_readonly)
            sock->async_read_some(
                    _buf(bf, _cfg.buffer_size_hint),
                    [this, id, sock, bf](auto&& a, auto&& b)
                    {
                        _handle_discarded(id, sock, bf, a, b);
                    });
        else
            asio::async_read(
                    *sock, _buf(bf, 8),
                    [this, id, sock, bf](auto&& a, auto&& b)
                    {
                        _handle_active_header(id, sock, bf, a, b);
                    });
    }

    void _handle_discarded(
            socket_id_t id, tcp::socket* sock, buffer_ptr bf,
            asio::error_code const& ec, size_t n_read)
    {
        if (ec)
            return;

        _perf_in(n_read);
        sock->async_read_some(
                _buf(bf, _cfg.buffer_size_hint),
                [this, id, sock, bf](auto&& a, auto&& b)
                {
                    _handle_discarded(id, sock, bf, a, b);
                });
    }

    void _handle_active_header(
            socket_id_t id, tcp::socket* sock, buffer_ptr bf,
            asio::error_code const& ec, size_t n_read)
    {
        auto size = _handle_header(id, sock, bf, ec, n_read);
        if (~size_t{} == size)
            return;

        asio::async_read(
                *sock, _buf(bf, size),
                [this, id, bf, sock](auto&& a, auto&& b)
                {
                    _handle_active_body(id, sock, bf, a, b);
                });
    }

    void _handle_active_body(
            socket_id_t id, tcp::socket* sock, buffer_ptr bf,
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
            auto buf = _view(bf, n_read);
            message  = nlohmann::json::from_msgpack(buf.begin(), buf.end());
            decltype(_recv_routes)::mapped_type* fn;
            {
                std::lock_guard lc{_mtx_modify};
                fn = &_recv_routes.at(message.route);
            }

            (*fn)(message.parameter);
        }
        catch (std::out_of_range& e)
        {
            CPPH_CRITICAL("INVALID ROUTE RECEIVED: {}", message.route);
        }
        catch (std::exception& e)
        {
            CPPH_ERROR("failed to parse input log message");
        }

        asio::async_read(
                *sock, _buf(bf, 8),
                [this, id, sock, bf](auto&& a, auto&& b)
                {
                    _handle_active_header(id, sock, bf, a, b);
                });
    }

    void _tick_perf()
    {
        if (_perf_timer())
        {
            _perf_in_rate  = _perf_bytes_in / _perf_timer.delta().count();
            _perf_out_rate = _perf_bytes_out / _perf_timer.delta().count();

            _perf_bytes_in = _perf_bytes_out = 0;
        }
    }

    void _perf_in(size_t n)
    {
        _tick_perf();
        _perf_bytes_in += n;
    }

    void _perf_out(size_t n)
    {
        _tick_perf();
        _perf_bytes_out += n;
    }

    size_t _retrieve_buffer_size(message_header_t h)
    {
        static constexpr message_header_t reference;

        // check header validity
        if (not std::equal(h.identifier, *(&h.identifier + 1), reference.identifier))
            return CPPH_WARN("identifier: {}", std::string_view{h.base64_buflen, 4}), ~size_t{};

        // check if header size does not exceed buffer length
        size_t decoded = 0;
        if (not base64::decode(h.base64_buflen, (char*)&decoded))
            return CPPH_WARN("decoding base64: {}", std::string_view{h.base64_buflen, 4}), ~size_t{};

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
    std::unordered_map<socket_id_t, std::unique_ptr<tcp::socket>> _sockets;
    std::unordered_map<socket_id_t, buffer_ptr> _buffers;
    std::vector<tcp::socket*> _sockets_active;

    std::atomic_bool _alive{false};
    std::thread _io_worker;

    poll_timer _perf_timer{1s};
    size_t _perf_bytes_out = 0;
    size_t _perf_bytes_in  = 0;
    size_t _perf_out_rate  = 0;
    size_t _perf_in_rate   = 0;

    std::string _token;      // login token to compare with.
    std::mutex _mtx_modify;  // locks when modification (socket add/remove) occurs.

    std::map<std::string, std::function<bool(recv_archive_type const& parameter)>>
            _recv_routes;

    std::vector<uint8_t> _bf_send;
    std::atomic_int _n_sending = 0;

    logger_ptr _logger = logging::find_or("PERFKIT:NET");
};
}  // namespace perfkit::terminal::net::detail