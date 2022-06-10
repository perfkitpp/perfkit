#pragma once
#include <cstdint>

namespace perfkit::v2 {
enum class edit_mode : uint8_t {
    none,

    trigger = 1,  // Usually boolean. On click, sends flipped value.

    path = 10,
    path_file = 11,
    path_file_multi = 12,
    path_dir = 13,

    script = 20,  // usually long text

    color_b = 31,  // maximum 4 ch, 0~255 color range per channel
    color_f = 32,  // maximum 4 ch, usually 0.~1. range. Channel can exceed 1.
};
}