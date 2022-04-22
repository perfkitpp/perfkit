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
#include <mutex>

#include "cpph/spinlock.hxx"

namespace perfkit::detail {
class config_base;
}

namespace perfkit::configs {
class watcher
{
   public:
    template <typename ConfTy_>
    bool check_dirty(ConfTy_ const& conf) const
    {
        return _check_update_and_consume(&conf.base());
    }

    template <typename ConfTy_>
    bool check_dirty_safe(ConfTy_ const& conf) const
    {
        auto _{std::lock_guard{_lock}};
        return _check_update_and_consume(&conf.base());
    }

   private:
    bool _check_update_and_consume(detail::config_base* ptr) const;

   public:
    mutable std::unordered_map<detail::config_base*, uint64_t> _table;
    mutable perfkit::spinlock _lock;
};
}  // namespace perfkit::configs