// MIT License
//
// Copyright (c) 2021-2022. Seungwoo Kang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// project home: https://github.com/perfkitpp

#pragma once
#include "net.hpp"
#include "perfkit/configs.h"

PERFKIT_CATEGORY(perfkit::terminal::net::flags)
{
    PERFKIT_CONFIGURE(name, "default")
            .transient()
            .hide()
            .flags("term-name")
            .confirm();

    PERFKIT_CONFIGURE(port, 0)
            .transient()
            .hide()
            .flags("term-port")
            .confirm();

    PERFKIT_CONFIGURE(auth, "")
            .transient()
            .hide()
            .flags("term-auth")
            .confirm();
}

namespace perfkit::terminal::net {
/**
 * Intentionally declared non-inline function, as this header must be included only for once. \n
 * Before calling this function, command line arguments must be parsed
 */
terminal_ptr create_from_flags()
{
    flags::update();

    terminal_init_info info{*flags::name};
    info.serve(*flags::port);
    info.parse_auth(*flags::auth);

    return create(info);
}

}  // namespace perfkit::terminal::net
