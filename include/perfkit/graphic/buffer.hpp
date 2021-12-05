#pragma once
#include <perfkit/common/hasher.hxx>

namespace perfkit::gui {
using mesh3d_key = basic_key<class LABEL_mesh3d_key>;
using image_key  = basic_key<class LABEL_mesh3d_key>;

/**
 * A buffer which contains a set of displayable data.
 *
 * - Render various set of static 2D glyphs
 * - Configurable 3D camera (camera parameters,
 * - Render 3D glyphs on world space (sphere, cube, etc ...)
 * - Render resources (3D modeling, image, etc ...). Resources are managed by integer key,
 *    and are cached in the client until the next invalidation.
 *    (e.g. buffer->upload(0x1234, pointer_to_resource_data))
 */
class graphic
{
    // set canvas size

    // move 3d camera

    // configure 3d camera

    // register 3d mesh resource

    // register 2d texture resource

    // draw 2d glyphs
    // - shapes (polygon, rectangle, circle, text, ...)
    // - bitblt/stretchblt textures

    // draw 3d glyphs
    // - shapes (lines, sphere, aabb, text, ...)
    // - 3d mesh resource, with transform, texture information ...

   private:
    friend class instance;
    void lock();  // interfaces lock_guard
    void unlock();
};

/**
 * A proxy class to expose resource creation operations only
 */
class resource_proxy
{
};

/**
 * A proxy class to expose drawing operations only
 */
class draw_proxy
{
};
}  // namespace perfkit::gui