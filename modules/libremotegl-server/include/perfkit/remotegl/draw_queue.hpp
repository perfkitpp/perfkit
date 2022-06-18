#pragma once
#include "handle.hpp"

namespace perfkit::rgl {
class draw_queue
{
   public:
    virtual ~draw_queue() = default;

   public:
    virtual void clear(vec3f) = 0;

    /**
     * Select resources
     */
    virtual void select(texture_handle) = 0;
    virtual void select(mesh_handle) = 0;
    virtual void select_fg(vec4f const&) = 0;
    virtual void select_bg(vec4f const&) = 0;
    virtual void select_mesh(array_view<vec3f> immed_poly) = 0;
    virtual void select_mesh(array_view<vec3f> immed_vert, array_view<int> immed_idx) = 0;

    /**
     * Performs stretch blt
     */
    virtual void stretch(rectanglef const& dst, rectanglef const& src) = 0;
    virtual void stretch(rectanglef const& dst) = 0;
    virtual void stretch() = 0;

    /**
     *
     */
};
}  // namespace perfkit::rgl
