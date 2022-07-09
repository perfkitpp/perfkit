#pragma once
#include "cpph/helper/macros.hxx"
#include "cpph/refl/core.hxx"

#define PERFKITINTERNAL_RGL_SERVERCMD(Command) \
    template <>                                \
    struct server_command<servercmd::Command> : command_base_class<server_command<servercmd::Command>>

#define PERFKITINTERNAL_RGL_CLIENTCMD(Command) \
    template <>                                \
    struct client_command<clientcmd::Command> : command_base_class<client_command<clientcmd::Command>>

namespace perfkit::rgl {
enum class servercmd {
    none,
    // ---------------------------------------------------------------------------------------------
    _generic = 0x0001,

    resource_registered = 0x0100,
    resource_unregistered = 0x101,

    _generic_end,
    // ---------------------------------------------------------------------------------------------
    _tex = 0x1000,

    tex_init = 0x1100,
    tex_update_jpeg = 0x1101,
    tex_update_raw = 0x1102,

    _tex_end,
    // ---------------------------------------------------------------------------------------------
    _gl = 0x2000,

    gl_select_texture = 0x2210,
    gl_select_mesh,
    gl_select_material,

    gl_wire_text = 0x2230,  // does not utilize brushes
    gl_wire_lines,
    gl_wire_sphere,
    gl_wire_cube,
    gl_wire_arrow,

    gl_render_object,
    gl_render_plane,
    gl_render_billboard,

    _gl_end,
    // ---------------------------------------------------------------------------------------------
    _gl2d = 0x2500,

    gl2d_bitblt = 0x2510,
    gl2d_stretch,

    gl2d_draw_text = 0x2530,

    _gl2d_end,
    // ---------------------------------------------------------------------------------------------
    _wnd = 0x3000,

    _wnd_end,
    // ---------------------------------------------------------------------------------------------
    _rsrc = 0x4000,

    _rsrc_end,
    // ---------------------------------------------------------------------------------------------
    _sound = 0x5000,

    _sound_end,
    // ---------------------------------------------------------------------------------------------
};

enum class clientcmd {
    none,

    _generic = 0x0001,
    subscribe_resource = 0x0011,
    cancel_subscription = 0x0012,

    _wnd = 0x3000,
};

template <servercmd>
struct server_command;

template <clientcmd>
struct client_command;

template <typename T>
struct command_base_class {
    using Self = T;
};
}  // namespace perfkit::rgl
