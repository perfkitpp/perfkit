/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2022. Seungwoo Kang
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
#include <memory>

#include <cpph/refl/core.hxx>
#include <cpph/utility/generic.hxx>

namespace perfkit {
using namespace cpph;
class if_terminal;
}  // namespace perfkit

namespace perfkit::web {
struct open_info {
    CPPH_REFL_DECLARE_c;

    uint16_t bind_port = 0;
    string bind_ip = "0.0.0.0";
    string alias = "default";
    string description = "";
    string secret_path = "~/.ssh/id_rsa";

    string static_dir = "perfkit.web.public";
};

auto open(open_info) -> shared_ptr<if_terminal>;
auto open(string_view str = "__PERFKIT_WEB__") -> shared_ptr<if_terminal>;
}  // namespace perfkit::web
