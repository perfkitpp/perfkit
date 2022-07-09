#pragma once
#include <cpph/utility/templates.hxx>

namespace perfkit::rgl {
enum class texel_type : uint8_t {
    b8 = 0,  // mono
    b16 = 1,
    b24 = 2,  // bgr, xyz
    b32 = 3,  // bgra, xyzw

    f32 = 4,  // mono
    f64 = 5,
    f96 = 6,  // bgr, xyz
    f128 = 7  // bgra, xyzw
};

template <texel_type T>
using texel_channel_type = conditional_t<T <= texel_type::b32, uint8_t, float>;

constexpr int texel_channels(texel_type t) noexcept { return (int)t % 4 + 1; }
constexpr int texel_extent(texel_type t) noexcept { return texel_channels(t) * (t <= texel_type::b32 ? 1 : 4); }
constexpr bool is_texel_float(texel_type t) noexcept { return (int)t & 1; }
}  // namespace perfkit::rgl
