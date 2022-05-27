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

//
// Created by ki608 on 2022-03-19.
//

#include "config_context.hpp"

#include <spdlog/spdlog.h>

#include "asio/io_context.hpp"
#include "asio/post.hpp"
#include "cpph/refl/object.hxx"
#include "cpph/refl/rpc/rpc.hxx"
#include "perfkit/detail/configs-v2-backend.hpp"
#include "perfkit/logging.h"

using namespace perfkit::net;
using std::forward;
using std::move;

namespace perfkit::net::detail {
spdlog::logger* nglog();
}

void config_context::start_monitoring(weak_ptr<void> anchor)
{
    _monitor_anchor = move(anchor);

    // TODO: Register all existing repositories
}

void config_context::stop_monitoring()
{
    _monitor_anchor.reset();

    // TODO: Reset all cached contexts
}

void config_context::rpc_republish_all_registries()
{

}

void config_context::rpc_update_request(const message::config_entity_update_t& content)
{
}

static auto CPPH_LOGGER()
{
    return detail::nglog();
}