#pragma once

namespace perfkit::rgl {
class draw_queue;

enum class texture_type {
    b8,   // mono
    b24,  // bgr, xyz
    b32,  // bgra, xyzw

    f32,  // mono
    f96,  // bgr, xyz
    f128  // bgra, xyzw
};
}  // namespace perfkit::rgl
