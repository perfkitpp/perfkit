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

#include "perfkit/extension/net.hpp"

#include "net_terminal.hpp"

namespace perfkit::terminal::net {
terminal_ptr create(const profile& cfg)
{
    perfkit::net::terminal_info init;
    init.name = cfg.session_name;
    init.description = cfg.session_description;
    init.bind_port = cfg.bind_port;
    init.bind_ip = cfg.bind_address;
    init.enable_find_me = cfg.enable_find_me;

    auto term = std::make_shared<perfkit::net::terminal>(std::move(init));
    term->_start_();

    return term;
}

perfkit::terminal_ptr create(std::string config_name)
{
    profile pf{std::move(config_name)};
    pf->update();
    return create(pf);
}

}  // namespace perfkit::terminal::net
