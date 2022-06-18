#pragma once
#include "handle.hpp"

namespace perfkit::rgl {

enum class sys_axis {
    x = 1,
    y = 2,
    z = 3,
};

enum class text_align_h {
    left = 0,
    center = 1,
    right = 2,
};

enum class text_align_v {
    up = 0,
    center = 1,
    down = 2
};

sys_axis operator+(sys_axis v) noexcept { return v; }
sys_axis operator-(sys_axis v) noexcept { return sys_axis(-int(v)); }

class draw_queue
{
   public:
    virtual ~draw_queue() = default;
    virtual void _internal_reset(render_target_handle h) = 0;

   public:
    /**
     * Clear buffer with given color.
     *
     * Dependency information will also be cleared.
     */
    virtual void clear(vec4f) = 0;

    /**
     * Retrieve resource dependencies
     */
    virtual void deps(std::vector<uint64_t>* o_deps) const = 0;

    /**
     * Set system axis
     *
     * e.g. In OpenCV, system axis is (z, x, -y)
     */
    virtual void axes(sys_axis fwd, sys_axis right, sys_axis up) = 0;
    void axes_opencv() { axes(+sys_axis::z, +sys_axis::x, -sys_axis::y); }
    void axes_opengl() { axes(-sys_axis::z, +sys_axis::x, +sys_axis::y); }
    void axes_unity() { axes(+sys_axis::z, +sys_axis::x, +sys_axis::y); }
    void axes_unreal() { axes(+sys_axis::x, +sys_axis::y, +sys_axis::z); }

    /**
     * Number of pushes/pops must be matching!
     */
    virtual void pop() = 0;

    /**
     * Select brushes
     *
     * - Another render target can be selected as brush. Only committed draw queue will be
     *   applied to final result.
     * - Brush affects to all sort of drawings ... Since
     */
    virtual void push_brush() = 0;
    virtual void brush(texture_handle) = 0;
    virtual void brush(render_target_handle) = 0;
    virtual void brush(material_handle) = 0;
    virtual void brush(vec4f const&) = 0;

    /**
     * Drawing properties
     */
    virtual void push_cfg() = 0;
    virtual void thickness(float value) = 0;                                // 1.0 default
    virtual void polydir(bool clockwise) = 0;                               // Polygon draw direction.
    virtual void wireframe(bool enable) = 0;                                // Disabled default
    virtual void text_align(text_align_h halign, text_align_v valign) = 0;  // Left is default

    /**
     * Mesh control
     */
    virtual void push_mesh() = 0;
    virtual void mesh(mesh_handle) = 0;
    virtual void mesh(array_view<vec3f> immed_vert, array_view<int> immed_idx) = 0;
    void mesh(array_view<vec3f> immed_poly);  // Element counts should be multiplicand of 3

    /**
     * Camera adjustment
     */
    virtual void push_world_offset() = 0;
    virtual void world_offset(vec3f pos, vec4f quat) = 0;

    /**
     * Performs stretch blt in screen space
     */
    virtual void stretch(rectanglef const& dst, rectanglef const& src) = 0;
    void stretch(rectanglef const& dst);
    void stretch();

    /**
     * Draw object using world position. If camera is placed in origin, position is interpreted as
     *  camera relative coordinate.
     */
    virtual void object(vec3f pos, vec4f const& quat) = 0;
    void object(vec3f pos, float yaw, float pitch, float roll);  // Applied in fixed order
    void object(vec3f pos, vec3f const& axis, float angle);
    void object(vec3f pos, vec3f const& rodrigues);

    /**
     * Draw 2D
     */
    virtual void move_to(vec2f pos) = 0;  // Move cursor position in screen space
    virtual void line_to(vec2f pos) = 0;  // Draw line to ...
    virtual void circle(float radius, bool filled) = 0;

    virtual void text(string_view content, float scale) = 0;  // Draw text on screen space. Scale 1 is reference.
};
}  // namespace perfkit::rgl
