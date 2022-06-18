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

struct resource_node {
    resource_sync_context sync = {};
    basic_handle handle = {};
    unique_ptr<void, void (*)(void*)> data;
};

class context_impl : public context
{
    vector<resource_node> _nodes;
    event_queue_worker _worker{thread::lazy, 8000};
    ptr<context_event_handler> _backend;

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
