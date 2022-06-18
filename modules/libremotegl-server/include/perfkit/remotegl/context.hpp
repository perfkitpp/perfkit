#pragma once
#include <string>
#include <string_view>

#include "cpph/math/rectangle.hxx"
#include "cpph/memory/pool.hxx"
#include "cpph/utility/template_utils.hxx"
#include "defs.hpp"
#include "handle.hpp"
#include "perfkit/remotegl/protocol.hpp"

namespace perfkit::rgl {
using namespace cpph;
using namespace cpph::math;
using std::string_view;

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
     * @param [out] o_hrt Creation result will be stored here.
     * @param [in] alias [optional] Client may distinguish render targets through alias. Can be empty
     * @return
     */
    virtual bool create(render_target_handle* o_hrt, vec2i size, string_view alias) = 0;

    /**
     * Create empty draw command queue on given render target.
     *
     * Uncommitted draw command queue will be discarded.
     */
    virtual unique_pool_ptr<draw_queue> begin_drawing(render_target_handle) = 0;

    /**
     * Commit draw queue content
     */
    virtual void commit(unique_pool_ptr<draw_queue>) = 0;

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
     * Create texture link to client
     */
    virtual bool create(texture_handle* o_htex, vec2i size, texture_type type, string_view alias) = 0;

    /**
     * Upload texture to client device.
     *
     * Actual upload operation will be performed when the texture is actually referenced.
     */
    virtual bool upload(texture_handle htex, array_view<void const> data, vec2i size, texture_type type) = 0;

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

    /**
     * Get backend class
     */
};
}  // namespace perfkit::rgl
