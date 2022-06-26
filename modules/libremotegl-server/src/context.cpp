#include "perfkit/remotegl/context.hpp"

#include "cpph/thread/thread_pool.hxx"
#include "cpph/utility/functional.hxx"
#include "perfkit/remotegl/draw_queue.hpp"
#include "resource.hpp"
#include "resource/texture.hpp"

namespace perfkit::rgl {
using std::vector;

struct resource_node {
    size_t empty_next = ~size_t{};

    basic_handle handle = {};
    ptr<basic_synced_resource> resource;
};

struct context::impl {
    // Node management
    size_t _empty_node = ~size_t{};
    size_t _idgen = 0;
    vector<resource_node> _nodes;

    // Event management
    cpph::event_queue_worker _worker{1 << 20};

    // Client
    shared_ptr<backend_client> _client;
    atomic<void const*> _address = nullptr;

   public:
    basic_handle new_node(resource_type t) noexcept
    {
        basic_handle h = {};
        h.id(++_idgen);
        h.type(t);

        if (_empty_node != ~size_t{}) {
            h.index(_empty_node);
            auto* p_node = &_nodes.at(_empty_node);
            assert(p_node->resource == nullptr);
            p_node->handle = h;
            p_node->empty_next = ~size_t{};

            _empty_node = p_node->empty_next;  // Point to next empty resource blk
        } else {
            assert(_nodes.size() < basic_handle::max_index());
            h.index(_nodes.size());
            _nodes.emplace_back().handle = h;
        }

        return h;
    }

    void dispose_node(basic_handle h) noexcept
    {
        auto p_node = get_node(h);
        assert(p_node->handle.value == h.value);

        p_node->resource.reset();
        p_node->handle = {};

        // Mark this node as 'idle'
        p_node->empty_next = _empty_node;
        _empty_node = h.index();
    }

    resource_node* get_node(basic_handle h) noexcept
    {
        auto* p_node = &_nodes[h.index()];
        assert(p_node->handle.value == h.value);

        return p_node;
    }
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

        // If client parameter is passed, load it and restart worker.
        if (client) {
            client->_bk_register(this);
            self->_client = move(client);
            self->_client->on_start_communication();
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