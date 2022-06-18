#pragma once
#include <string>
#include <string_view>

#include "cpph/container/buffer.hxx"
#include "cpph/math/rectangle.hxx"
#include "cpph/memory/pool.hxx"
#include "cpph/utility/template_utils.hxx"
#include "defs.hpp"
#include "handle.hpp"
#include "perfkit/remotegl/protocol/common.hpp"

namespace perfkit::rgl {
using namespace cpph;
using namespace cpph::math;
using std::string_view;

/**
 * Describes texture information
 */
struct texture_metadata {
    vec2i size;
    texture_type texel_type;
    bool is_lossy = false;  // Can be lossy?
    bool is_video = false;  // Is uploaded texture relevant to previous ?

    texture_metadata(
            const vec2i& size, texture_type texel_type, bool is_lossy = false, bool is_video = false) noexcept
            : size(size), texel_type(texel_type), is_lossy(is_lossy), is_video(is_video) {}
};

/**
 * Defines context backend event handler
 */
class context_event_handler
{
   public:
    virtual ~context_event_handler() = default;
    virtual void start_event_handler() {}
    virtual void upload_texture(uint64_t handle, texture_metadata, flex_buffer const&) {}
    virtual void commit_draw_commands(flex_buffer const&) {}
    // virtual void upload_mesh() {} // TODO
};

/**
 * - Creating render target/ Committing draw command queue of render target
 * - Uploading resources (texture/mesh/etc ...)
 */
class context
{
   public:
    virtual ~context() = default;

    /**
     * Create new empty rgl context.
     */
    static auto create_new() -> ptr<context>;

    /**
     * Get singleton interface of rgl context.
     *
     * If non was exist, this creates empty one. Many terminal instances may use this
     *  global instance to provide remote gl service
     */
    static context* get();

   public:
    /**
     * Create an empty render target. Render target is always f128 bgra.
     *
     * @param [in] alias [optional] Client may distinguish render targets through alias. Can be empty
     */
    virtual render_target_handle create_render_target(vec2i size, string_view alias) = 0;

    /**
     * Create empty draw command queue on given render target.
     *
     * Uncommitted draw commands will be discarded.
     *
     * @warning Methods of draw_queue is not reentrant!
     */
    virtual draw_queue* begin_drawing(render_target_handle) = 0;

    /**
     * Commit draw queue content and swap buffer.
     */
    virtual void end_drawing(draw_queue*) = 0;

    /**
     * Dispose render target
     */
    virtual bool dispose(render_target_handle) = 0;

    /**
     * Create empty window. Size and position may change during run.
     *
     * Only initial window size and position can be specified here.
     */

    /**
     * Get event list of given window
     */

    /**
     * Create texture link to client. Size may vary.
     *
     * @param size Size of image
     */
    virtual texture_handle create_texture(texture_metadata const&, string_view alias) = 0;

    /**
     * Upload texture to client device.
     *
     * Actual upload operation will be performed when the texture is actually referenced.
     */
    virtual bool upload(texture_handle htex, array_view<void const> data) = 0;

    /**
     * Dispose texture. Any use after disposal will make invalid rendering result of client
     */
    virtual bool dispose(texture_handle) = 0;

    /**
     * Upload 3D mesh data to client device.
     */

    /**
     * Read 3D mesh data from .obj file
     */

   public:
    virtual void backend_register(ptr<context_event_handler> ptr) = 0;
    virtual void backend_subscribe(uint64_t raw_handle) = 0;
    virtual void backend_unsubscribe(uint64_t raw_handle) = 0;
    virtual void backend_ready(uint64_t raw_handle, size_t fence_ready) = 0;
};
}  // namespace perfkit::rgl
