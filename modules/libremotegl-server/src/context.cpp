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
   private:
    size_t _empty_node = ~size_t{};
    size_t _idgen = 0;
    vector<resource_node> _nodes;

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
{
}

context* context::get()
{
    return nullptr;
}

texture_handle context::create_texture(const texture_metadata& meta, string_view alias)
{
    return perfkit::rgl::texture_handle();
}

bool context::upload(texture_handle htex, array_view<const void> data)
{
    return false;
}

bool context::dispose(texture_handle)
{
    return false;
}

}  // namespace perfkit::rgl