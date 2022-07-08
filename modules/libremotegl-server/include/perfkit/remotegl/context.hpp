#pragma once
#include <string>
#include <string_view>

#include "backend.hpp"
#include "cpph/container/buffer.hxx"
#include "cpph/math/rectangle.hxx"
#include "cpph/memory/pool.hxx"
#include "cpph/utility/functional.hxx"
#include "cpph/utility/generic.hxx"
#include "defs.hpp"
#include "handle.hpp"
#include "perfkit/remotegl/protocol/common.hpp"

namespace perfkit::rgl {
using namespace cpph;
using namespace cpph::math;
using std::string_view;
class context;

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
 * - Creating render target/ Committing draw command queue of render target
 * - Uploading resources (texture/mesh/etc ...)
 */
class context : public if_graphics_backend
{
    struct impl;
    ptr<impl> self;

   public:
    context();
    ~context();
    context(context&&) noexcept = default;

    /**
     * Get singleton instance of rgl context.
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
    render_target_handle create_render_target(vec2i size, string_view alias);

    /**
     * Create empty draw command queue on given render target.
     *
     * Uncommitted draw commands will be discarded.
     *
     * @warning Methods of draw_queue is not reentrant!
     */
    draw_queue* begin_drawing(render_target_handle);

    /**
     * Commit draw queue content and swap buffer.
     */
    void commit_drawing(draw_queue*);

    /**
     * Dispose render target
     */
    bool dispose(render_target_handle);

    /**
     * Create empty window.
     *
     * Initial window size and position can be specified here.
     */

    /**
     * Get event list from given window
     */

    /**
     * Request window move/resize/close/hide/maximize...etc
     */

    /**
     * Create texture link to client. Size may vary.
     *
     * @param size Size of image
     */
    texture_handle create_texture(texture_metadata const&, string_view alias);

    /**
     * Upload texture to client device.
     *
     * Actual upload operation will be performed when the texture is actually referenced.
     */
    bool upload(texture_handle htex, array_view<void const> data);

    /**
     * Dispose texture. Any use after disposal will make invalid rendering result of client
     */
    bool dispose(texture_handle);

    /**
     * Upload 3D mesh data to client device.
     */

    /**
     * Read 3D mesh data from .obj file
     */

   public:
    void on_client_message(void const*, const_buffer_view view) override;
    void on_client_disconnection() override;

   public:
    void _bk_register(shared_ptr<backend_client> client = nullptr);
};
}  // namespace perfkit::rgl
