#pragma once

namespace perfkit::rgl {
class draw_queue;

enum class texture_type {
    b8,   // mono
    b24,  // bgr
    b32,  // bgra

    f32,  // mono
    f96,  // bgr
    f128  // bgra
};
}  // namespace perfkit::rgl
