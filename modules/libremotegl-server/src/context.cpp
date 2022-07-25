#include "perfkit/remotegl/context.hpp"

#include "cpph/refl/archive/msgpack-writer.hxx"
#include "cpph/refl/object.hxx"
#include "cpph/streambuf/string.hxx"
#include "cpph/thread/event_wait.hxx"
#include "cpph/thread/thread_pool.hxx"
#include "cpph/utility/functional.hxx"
#include "perfkit/remotegl/draw_queue.hpp"
#include "perfkit/remotegl/protocol/command.generic.hpp"
#include "perfkit/remotegl/protocol/command.tex.hpp"
#include "resource.hpp"
#include "resource/texture.hpp"

namespace perfkit::rgl {
using std::vector;

struct resource_node {
    size_t _empty_next = ~size_t{};

    string alias;

    basic_resource_handle handle = {};
    ptr<basic_synced_resource> resource;
};

struct context::impl {
    // Node management
    size_t _empty_node = ~size_t{};
    size_t _idgen = 0;
    vector<resource_node> _nodes;

    // Event management
    event_queue_worker _worker{1 << 20};

    // Client
    shared_ptr<backend_client> _client;
    atomic<void const*> _address = nullptr;

    // Buffer
    streambuf::stringbuf _cmd_buf;
    archive::msgpack::writer _cmd_write{&_cmd_buf};

   public:
    basic_resource_handle new_node(resource_type t) noexcept
    {
        basic_resource_handle h = {};
        h.id(++_idgen);
        h.type(t);

        if (_empty_node != ~size_t{}) {
            h.index(_empty_node);
            auto* p_node = &_nodes.at(_empty_node);
            assert(p_node->resource == nullptr);
            p_node->handle = h;
            p_node->_empty_next = ~size_t{};

            _empty_node = p_node->_empty_next;  // Point to next empty resource blk
        } else {
            assert(_nodes.size() < basic_resource_handle::max_index());
            h.index(_nodes.size());
            _nodes.emplace_back().handle = h;
        }

        return h;
    }

    void dispose_node(basic_resource_handle h) noexcept
    {
        if (auto p_node = get_node(h)) {
            assert(p_node->handle.value == h.value);

            p_node->resource.reset();
            p_node->handle = {};

            // Mark this node as 'idle'
            p_node->_empty_next = _empty_node;
            _empty_node = h.index();
        }
    }

    resource_node* get_node(basic_resource_handle h) noexcept
    {
        auto* p_node = &_nodes[h.index()];
        if (p_node->handle.id() != h.id())
            return nullptr;
        else
            return p_node;
    }

    resource_node& at(basic_resource_handle h) noexcept
    {
        auto* p_node = &_nodes[h.index()];
        assert(p_node->handle.value == h.value);
        return *p_node;
    }

    void flush() noexcept
    {
        if (_client) {
            _cmd_write.flush();
            _client->on_message_to_client(const_buffer_view(_cmd_buf.strview()));
        }

        _cmd_buf.clear();
        _cmd_write.clear();
    }

    template <servercmd Command>
    impl& operator<<(server_command<Command> const& cmd)
    {
        _cmd_write.array_push(2);
        _cmd_write << Command << cmd;
        _cmd_write.array_pop();

        return *this;
    }

   public:  // Handlers
};

context::context() : self(make_unique<impl>())
{
}

context::~context()
        = default;

context* context::get()
{
    static context _instance;
    return &_instance;
}

texture_handle context::create_texture(const texture_metadata& meta, string_view alias)
{
    thread::trigger_wait wait;
    wait.prepare();
    texture_handle handle;

    self->_worker.dispatch([&] {
        // Create new node
        auto h_tex = self->new_node(resource_type::texture);
        handle.value = h_tex.value;

        // TODO: Create new texture
        auto& n = self->at(h_tex);
        n.resource = nullptr;  // TODO;

        // TODO: Send register command

        wait.trigger();
    });

    wait.wait();
    return {};
}

bool context::upload(texture_handle htex, array_view<const void> data)
{
    return false;
}

bool context::dispose(texture_handle)
{
    return false;
}

void context::_bk_register(shared_ptr<backend_client> client)
{
    release(self->_address, client.get());

    self->_worker.dispatch([this, client = move(client)]() mutable {
        // Notify disconnection to active client
        if (auto pclient = exchange(self->_client, {})) {
            pclient->on_close_communication();
            pclient->_bk_register(nullptr);
        }

        // Clear current command buffer
        self->_cmd_write.clear();
        self->_cmd_buf.clear();

        // If client parameter is passed, load it and restart worker.
        if (client) {
            client->_bk_register(this);
            self->_client = move(client);
            self->_client->on_start_communication();

            // Register all existing resources ...
            server_command<servercmd::resource_registered> cmd;
            for (auto& node : self->_nodes) {
                if (not node.resource)
                    continue;

                cmd.alias = node.alias;
                cmd.new_handle = node.handle;

                *self << cmd;
            }

            self->flush();
        }
    });
}

void context::on_client_disconnection()
{
}

void context::on_client_message(const void* addr, const_buffer_view view)
{
    //! Prevent late event invocation
    if (acquire(self->_address) != addr)
        return;
}

}  // namespace perfkit::rgl