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

//
// Created by ki608 on 2021-12-25.
//

#include "perfkit/graphics/storage.hpp"

#include "handle_registry.hxx"

using namespace perfkit::graphics;

namespace perfkit::graphics::detail {
// - Texture
//    베이스 버퍼 + 드로 콜
//
// - Window
//    텍스쳐와 이벤트의 결합.
//

}  // namespace perfkit::graphics::detail

class storage::impl
{
   private:
    detail::handle_registry _handles;

   public:
    void release(handle_data const& handle)
    {
    }

    handle_data find_resource(std::string_view key) const { return {}; }
    texture_handle create_texture(std::string key, bool lossless) { return {}; }
    window_handle create_window(std::string key) { return {}; }
    mesh_handle create_mesh(std::string key) { return {}; }

   public:
    void draw(
            texture_handle handle,
            size2 size, pixel_format format,
            void const* buffer,
            texture_draw_fn const& draw_fn) {}

    bool try_draw(
            texture_handle handle,
            size2 size, pixel_format format,
            void const* buffer,
            texture_draw_fn const& draw_fn) { return false; }

    void show(
            window_handle handle,
            window_proc_fn const& wnd_proc)
    {
    }

    bool try_show(
            window_handle handle,
            window_proc_fn const& wnd_proc) { return false; }

    modal_result
    modal(window_handle handle,
          modal_type type,
          window_proc_fn const& wnd_proc) { return modal_result::expired; }

   public:
    void _bk_start_session() {}
    void _bk_stop_session() {}
    void _bk_reset_caches() {}
    void _bk_retrieve_updates(std::string* out) {}
    void _bk_change_subscription_state(handle_data const& key, bool new_state) {}
};

perfkit::graphics::storage::storage() : self{new impl} {}
perfkit::graphics::storage::~storage() = default;

void perfkit::graphics::storage::release(const handle_data& handle)
{
    self->release(handle);
}
handle_data perfkit::graphics::storage::find_resource(std::string_view key) const
{
    return self->find_resource(key);
}
texture_handle perfkit::graphics::storage::create_texture(std::string key, bool lossless)
{
    return self->create_texture(std::move(key), lossless);
}
window_handle perfkit::graphics::storage::create_window(std::string key)
{
    return self->create_window(std::move(key));
}
mesh_handle perfkit::graphics::storage::create_mesh(std::string key)
{
    return self->create_mesh(std::move(key));
}
void perfkit::graphics::storage::draw(texture_handle handle, size2 size, pixel_format format, const void* buffer, texture_draw_fn const& draw_fn)
{
    self->draw(handle, size, format, buffer, draw_fn);
}
bool perfkit::graphics::storage::try_draw(texture_handle handle, size2 size, pixel_format format, const void* buffer, texture_draw_fn const& draw_fn)
{
    return self->try_draw(handle, size, format, buffer, draw_fn);
}
void perfkit::graphics::storage::show(window_handle handle, window_proc_fn const& wnd_proc)
{
    self->show(handle, wnd_proc);
}
bool perfkit::graphics::storage::try_show(window_handle handle, window_proc_fn const& wnd_proc)
{
    return self->try_show(handle, wnd_proc);
}
modal_result perfkit::graphics::storage::modal(window_handle handle, modal_type type, window_proc_fn const& wnd_proc)
{
    return self->modal(handle, type, wnd_proc);
}
void perfkit::graphics::storage::_bk_start_session()
{
    self->_bk_start_session();
}
void perfkit::graphics::storage::_bk_stop_session()
{
    self->_bk_stop_session();
}
void perfkit::graphics::storage::_bk_reset_caches()
{
    self->_bk_reset_caches();
}
void perfkit::graphics::storage::_bk_retrieve_updates(std::string* out)
{
    self->_bk_retrieve_updates(out);
}
void perfkit::graphics::storage::_bk_change_subscription_state(const handle_data& key, bool new_state)
{
    self->_bk_change_subscription_state(key, new_state);
}
