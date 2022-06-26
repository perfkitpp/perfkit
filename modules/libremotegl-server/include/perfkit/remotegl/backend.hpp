#pragma once
#include <memory>

#include "cpph/thread/threading.hxx"
#include "cpph/utility/array_view.hxx"

namespace perfkit {
using namespace cpph;
}

namespace perfkit::rgl {

/**
 * Backend
 */
class if_graphics_backend
{
   public:
    virtual ~if_graphics_backend() = default;

   public:
    //! When
    virtual void on_client_message(void const* address, const_buffer_view) = 0;
    virtual void on_client_disconnection() = 0;
};

/**
 * Defines context backend event handler
 */
class backend_client
        : public std::enable_shared_from_this<backend_client>
{
    std::atomic<if_graphics_backend*> _bk = nullptr;

   public:
    virtual ~backend_client() = default;

    //! Called when actual communication begins.
    virtual void on_start_communication() {}

    //! Called when communication must be disposed.
    //! This is guaranteed not to be called before user request.
    virtual void on_close_communication() {}

    //! Called on every server side message.
    virtual void on_message_to_client(const_buffer_view) = 0;

   public:
    void message_to_server(const_buffer_view v)
    {
        if (auto bk = acquire(_bk))
            bk->on_client_message(this, v);
    }

    void notify_disconnection()
    {
        if (auto bk = acquire(_bk))
            bk->on_client_disconnection();
    }

   public:
    void _bk_register(if_graphics_backend* bk)
    {
        _bk = bk;
    }
};

}  // namespace perfkit::rgl