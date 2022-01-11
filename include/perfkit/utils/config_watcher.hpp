#pragma once
#include <mutex>

#include "perfkit/common/spinlock.hxx"

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