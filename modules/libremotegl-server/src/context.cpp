#include "perfkit/remotegl/context.hpp"

#include "cpph/thread/thread_pool.hxx"
#include "cpph/utility/functional.hxx"
#include "perfkit/remotegl/draw_queue.hpp"

namespace perfkit::rgl {
using std::vector;

/**
 * Synchronization status of individual resources
 */
struct resource_sync_context {
    size_t fence_client_sent = 0;
    size_t fence_client_ready = 0;
    size_t fence = 0;

    bool is_actively_subscribed = false;
};

using resource_data_ptr = unique_ptr<void, void (*)(void*)>;

struct resource_node {
    resource_sync_context sync = {};
    basic_handle handle = {};
    resource_data_ptr data{nullptr, nullptr};
};

class context_impl : public context
{
    vector<resource_node> _nodes;
    event_queue_worker _worker{thread::lazy, 8000};
    ptr<context_event_handler> _backend;
    uint64_t _idgen = 0;

   public:
    render_target_handle create_render_target(vec2i size, string_view alias) override
    {
        return {};
    }

    draw_queue* begin_drawing(render_target_handle handle) override
    {
        return nullptr;
    }

    void end_drawing(draw_queue* queue) override
    {
    }

    bool dispose(render_target_handle handle) override
    {
        return false;
    }

    texture_handle create_texture(const texture_metadata& metadata, string_view alias) override
    {
        return perfkit::rgl::texture_handle();
    }

    bool upload(texture_handle htex, array_view<const void> data) override
    {
        return false;
    }

    bool dispose(texture_handle handle) override
    {
        return false;
    }

   public:
    ~context_impl() override
    {
        context_impl::backend_register(nullptr);
    }

   public:
    void backend_register(ptr<context_event_handler> ptr) override
    {
        _worker.clear();
        _worker.shutdown();

        // Only worker thread accesses data members ...
        // In this context, as worker thread stopped, can safely access to _nodes.
        for (auto& node : _nodes) {
            node.sync.is_actively_subscribed = false;
            node.sync.fence_client_ready = 0;
            node.sync.fence_client_sent = 0;
        }

        if ((_backend = move(ptr)) != nullptr) {
            _worker.launch();
            _worker.clear();
            _worker.post([this] { _backend->start_event_handler(); });
        }
    }

    void backend_subscribe(uint64_t raw_handle) override
    {
        assert(_backend != nullptr);
    }

    void backend_unsubscribe(uint64_t raw_handle) override
    {
        assert(_backend != nullptr);
    }

    void backend_ready(uint64_t raw_handle, size_t fence_ready) override
    {
        assert(_backend != nullptr);
    }

   private:
    basic_handle _create_empty_handle(resource_type t, resource_data_ptr data)
    {
        basic_handle r = {};
        resource_node* node = {};

        r.id(++_idgen);
        r.type(t);

        for (auto& n : _nodes) {
            if (n.data == nullptr) {
                r.index(uint64_t(&n - _nodes.data()));
                break;
            }
        }

        if (_nodes.size() > 65535) {
            throw std::runtime_error{"All slot used!"};
        }

        if (node == nullptr) {
            r.index(_nodes.size());
            node = &_nodes.emplace_back();
        }

        node->sync = {};
        node->handle = r;
        node->data = move(data);
        return r;
    }

    resource_node* _slot(uint64_t h)
    {
        auto& handle = reinterpret_cast<basic_handle&>(h);
        auto& node = _nodes.at(handle.index());

        if (node.handle.id() == handle.id()) {
            return &node;
        } else {
            return nullptr;
        }
    }

    bool _dispose(uint64_t h)
    {
        auto& handle = reinterpret_cast<basic_handle&>(h);
        auto& node = _nodes.at(handle.index());

        if (node.handle.id() == handle.id()) {
            node.handle = {};
            node.data.reset();
            return true;
        } else {
            return false;
        }
    }
};
}  // namespace perfkit::rgl

namespace perfkit::rgl {
context* context::get()
{
    static auto _inst = context::create_new();
    return _inst.get();
}

auto context::create_new() -> ptr<context>
{
    return make_unique<context_impl>();
}
}  // namespace perfkit::rgl
