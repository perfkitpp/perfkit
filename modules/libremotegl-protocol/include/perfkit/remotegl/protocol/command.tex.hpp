#pragma once
#include "basic_resource_handle.hpp"
#include "command.defs.hpp"
#include "cpph/refl/object.hxx"
#include "cpph/refl/types/binary.hxx"
#include "perfkit/remotegl/texture.defs.hpp"

namespace perfkit::rgl {
enum class texture_transfer_method : uint8_t {
    none = 0,
    raw = 1,
    jpeg = 2,
};

//
// Called when new texture is initiated.
PERFKITINTERNAL_RGL_SERVERCMD(tex_init)
{
    friend CPPH_REFL_DEFINE_PRIM_binary(Self);
    basic_resource_handle handle;
    texture_transfer_method transfer_method;

    int32_t width, height;
    texel_type type;
};

//
// Called when texture is updated with jpeg
PERFKITINTERNAL_RGL_SERVERCMD(tex_update_jpeg)
{
    CPPH_REFL_DEFINE_TUPLE_inline((), data);
    basic_resource_handle handle;

    // All required JSON data is just included in it. If resized image is delivered,
    // resize it again.
    flex_buffer data;
};

//
// Called texture raw data transfer
PERFKITINTERNAL_RGL_SERVERCMD(tex_update_raw)
{
    CPPH_REFL_DEFINE_TUPLE_inline((), actual_width, actual_height, data);

    // Image could be resized.
    int32_t actual_width, actual_height;

    // Raw data included in 'texture_type' units
    flex_buffer data;
};

}  // namespace perfkit::rgl
