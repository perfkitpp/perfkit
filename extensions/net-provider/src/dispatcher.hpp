//
// Created by ki608 on 2021-11-21.
//

#pragma once
#include <memory>

namespace perfkit::terminal::net::detail {
class basic_dispatcher_impl;
}

namespace perfkit::terminal::net {

class dispatcher {
  using impl_ptr = std::unique_ptr<detail::basic_dispatcher_impl>;

 public:
  dispatcher(impl_ptr ptr);
  ~dispatcher();

  template <typename MsgTy_,
            typename HandlerFn_,
            typename = std::enable_if_t<std::is_invocable_v<HandlerFn_, MsgTy_>>>
  void on_recv(std::string_view route, HandlerFn_&& handler) {
  }

  template <typename MsgTy_>
  void send(std::string_view route, int64_t fence, MsgTy_&& message) {
  }

 private:
  std::unique_ptr<detail::basic_dispatcher_impl> self;
};

}  // namespace perfkit::terminal::net
