/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2021. Seungwoo Kang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * project home: https://github.com/perfkitpp
 ******************************************************************************/

#pragma once
#include <algorithm>

namespace perfkit {
enum class termcolors;

/** Color info */
struct termcolor
{
    union
    {
        uint32_t code;
        struct
        {
            uint8_t r, g, b, a;
        };
    };

    explicit constexpr termcolor(
            uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) noexcept
            : termcolor((alpha << 24) + (blue << 16) + (green << 8) + (red)) {}

    explicit constexpr termcolor(int hex) noexcept
            : code(hex) {}

    constexpr termcolor(termcolors hex) noexcept  // intentionally implicit
            : code((uint32_t)hex + (0xff << 24))
    {
    }

    constexpr termcolor() noexcept
            : termcolor(0) {}

    constexpr termcolor(termcolor&&) noexcept      = default;
    constexpr termcolor(termcolor const&) noexcept = default;
    constexpr termcolor& operator=(termcolor const&) noexcept = default;
    constexpr termcolor& operator=(termcolor&&) noexcept = default;

    constexpr bool operator==(termcolor const& r) const noexcept { return code == r.code; }
    constexpr bool operator!=(termcolor const& r) const noexcept { return code != r.code; }
    constexpr bool operator<(termcolor const& r) const noexcept { return code < r.code; }

    template <typename OutIt_>
    void append_xterm_256(OutIt_ out, bool fg = true) noexcept
    {
        char buf[16];
        auto n_written = snprintf(buf, sizeof buf, "\033[%d;5;%dm", fg ? 38 : 48, xterm_256());
        std::copy_n(buf, n_written + 1, out);  // include null character
    }

    constexpr uint8_t xterm_256() const noexcept
    {
        if (code == 0) { return 7; }  // default color is normal white
        // scale channels in range 0-5, by dividing them ((256/6) + 1).floor()
        return 16 + 36 * (r / 43) + 6 * (g / 43) + (b / 43);
    }
};

/// See extended CSS color keywords (4.3) in http://www.w3.org/TR/2011/REC-css3-color-20110607/
enum class termcolors
{
    alice_blue             = 0xf0f8ff,
    antique_white          = 0xfaebd7,
    aqua                   = 0xFFFF,
    aquamarine             = 0x7fffd4,
    azure                  = 0xf0ffff,
    beige                  = 0xf5f5dc,
    bisque                 = 0xffe4ce,
    black                  = 0x0,
    blanched_almond        = 0xffebcd,
    blue                   = 0x0000FF,
    blue_violet            = 0x8a2be2,
    brown                  = 0xa52a2a,
    burly_wood             = 0xdeb887,
    cadet_blue             = 0x5f9ea0,
    chartreuse             = 0x7fff00,
    chocolate              = 0xd2691e,
    coral                  = 0xff7f50,
    cornflower_blue        = 0x6495ed,
    cornsilk               = 0xfff8dc,
    crimson                = 0xdc143c,
    cyan                   = 0xffff,
    dark_blue              = 0x8b,
    dark_cyan              = 0x8b8b,
    dark_goldenrod         = 0xb8860b,
    dark_gray              = 0xa9a9a9,
    dark_green             = 0x6400,
    dark_grey              = dark_gray,
    dark_khaki             = 0xbdb76b,
    dark_magenta           = 0x8b008b,
    dark_olive_green       = 0x556b2f,
    dark_orange            = 0xff8c00,
    dark_orchid            = 0x9932cc,
    dark_red               = 0x8b0000,
    dark_salmon            = 0xe9976a,
    dark_sea_green         = 0x8fbc8f,
    dark_slate_blue        = 0x483d8b,
    dark_slate_gray        = 0x2f4f4f,
    dark_slate_grey        = 0x2f4f4f,
    dark_turquoise         = 0xced1,
    dark_violet            = 0x9400d3,
    deep_pink              = 0xff1493,
    deep_sky_blue          = 0xbfff,
    dim_gray               = 0x696969,
    dim_grey               = dim_gray,
    dodger_blue            = 0x1e90ff,
    firebrick              = 0xb22222,
    floral_white           = 0xfffaf0,
    forest_green           = 0x228b22,
    fuchsia                = 0xFF00FF,
    gainsboro              = 0xdcdcdc,
    ghost_white            = 0xf8f8ff,
    gold                   = 0xffd700,
    goldenrod              = 0xdaa520,
    gray                   = 0x808080,
    green                  = 0x008000,
    green_yellow           = 0xadff2f,
    grey                   = gray,
    honeydew               = 0xf0fff0,
    hot_pink               = 0xff69b4,
    indian_red             = 0xcd5c5c,
    indigo                 = 0x4b0082,
    ivory                  = 0xfffff0,
    khaki                  = 0xf0e68c,
    lavendar               = 0xe6e6fa,
    lavender_blush         = 0xfff0f5,
    lawn_green             = 0x7cfc00,
    lemon_chiffon          = 0xfffacd,
    light_blue             = 0xadd8e6,
    light_coral            = 0xf08080,
    light_cyan             = 0xe0ffff,
    light_goldenrod_yellow = 0xfafad2,
    light_gray             = 0xd3d3d3,
    light_green            = 0x90ee90,
    light_grey             = light_gray,
    light_pink             = 0xffb6c1,
    light_salmon           = 0xffa07a,
    light_sea_green        = 0x20b2aa,
    light_sky_blue         = 0x87cefa,
    light_slate_gray       = 0x778899,
    light_slate_grey       = light_slate_gray,
    light_steel_blue       = 0xb0c4de,
    light_yellow           = 0xffffe0,
    lime                   = 0x00FF00,
    lime_green             = 0x32cd32,
    linen                  = 0xfaf0e6,
    magenta                = 0xff00ff,
    maroon                 = 0x800000,
    medium_aquamarine      = 0x66cdaa,
    medium_blue            = 0xcd,
    medium_orchid          = 0xba55d3,
    medium_purple          = 0x9370db,
    medium_sea_green       = 0x3cb371,
    medium_slate_blue      = 0x7b68ee,
    medium_spring_green    = 0xfa9a,
    medium_turquoise       = 0x48d1cc,
    medium_violet_red      = 0xc71585,
    midnight_blue          = 0x191970,
    mint_cream             = 0xf5fffa,

    misty_rose      = 0xffe4e1,
    moccasin        = 0xffe4b5,
    navajo_white    = 0xffdead,
    navy            = 0x000080,
    old_lace        = 0xfdf5e6,
    olive           = 0x808000,
    olive_drab      = 0x6b8e23,
    orange          = 0xffa500,
    orange_red      = 0xff4500,
    orchid          = 0xda70d6,
    pale_goldenrod  = 0xeee8aa,
    pale_green      = 0x98fb98,
    pale_turquoise  = 0xafeeee,
    pale_violet_red = 0xdb7093,
    papaya_whip     = 0xffefd5,
    peach_puff      = 0xffdab9,
    peru            = 0xcd853f,
    pink            = 0xffc0cb,
    plum            = 0xdda0dd,
    powder_blue     = 0xb0e0e6,
    purple          = 0x800080,
    red             = 0xFF0000,
    rosy_brown      = 0xbc8f8f,
    royal_blue      = 0x4169e1,
    saddle_brown    = 0x8b4513,
    salmon          = 0xfa8072,
    sandy_brown     = 0xf4a460,
    sea_green       = 0x2e8b57,
    sea_shell       = 0xfff5ee,
    sienna          = 0xa0522d,
    silver          = 0xc0c0c0,
    sky_blue        = 0x87ceeb,
    slate_blue      = 0x6a5acd,
    slate_gray      = 0x708090,
    slate_grey      = 0x708090,
    snow            = 0xfffafa,
    spring_green    = 0xff7f,
    steel_blue      = 0x4682b4,
    tan             = 0xd2b48c,
    teal            = 0x008080,
    thistle         = 0xd8bfd8,
    tomato          = 0xff6347,
    turquoise       = 0x40e0d0,
    violet          = 0xee82ee,
    wheat           = 0xf5deb3,
    white           = 0xFFFFFF,
    white_smoke     = 0xf5f5f5,
    yellow          = 0xFFFF00,
    yellow_green    = 0x9acd32,
};

}  // namespace perfkit
