#pragma once
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
    virtual void on_client_message(const_buffer_view) = 0;
    virtual void on_client_disconnection() = 0;
};

/**
 * Defines context backend event handler
 */
class backend_context_event_handler
{
    if_graphics_backend* _bk = nullptr;

   public:
    virtual ~backend_context_event_handler() = default;

    //! Called when actual communication begins.
    virtual void on_start_communication() {}

    //! Called when communication must be disposed.
    virtual void on_close_communication() {}

    //! Called on every server side message.
    virtual void on_message_to_client(const_buffer_view) = 0;

   protected:
    void message_to_server(const_buffer_view v) { _bk->on_client_message(v); }
    void notify_disconnection() { _bk->on_client_disconnection(); }

   public:
    void _bk_register(if_graphics_backend* bk) { _bk = bk; }
};

}  // namespace perfkit::rgl