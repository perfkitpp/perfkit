#pragma once
#include <perfkit/common/hasher.hxx>

namespace perfkit::gui {
using mesh3d_key = key_base<class LABEL_mesh3d_key>;
using image_key  = key_base<class LABEL_mesh3d_key>;

/**
 * A buffer which contains a set of displayable data.
 *
 * - Render various set of static 2D glyphs
 * - Configurable 3D camera (camera parameters,
 * - Render 3D glyphs on world space (sphere, cube, etc ...)
 * - Render resources (3D modeling, image, etc ...). Resources are managed by integer key,
 *    and are cached in the client until next invalidation.
 *    (e.g. buffer->upload(0x1234, pointer_to_resource_data))
 * - IMGUI-styled immediate ui support & primitive layout system
 *
 */
class buffer {
};
}  // namespace perfkit::gui