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
#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "fwd.hpp"
#include "handle.hpp"
#include "types.hpp"

namespace perfkit::graphics {
/**
 * 그래픽스 처리에 관련된 모든 문맥(리소스, 윈도우 등)을 저장합니다.
 */
class storage : std::enable_shared_from_this<storage>
{
   public:
    storage();
    ~storage();

   public:
    /**
     * Release resource
     */
    void release(handle_data const& handle);

    /**
     * Find handle
     */
    handle_data find_resource(std::string_view key) const;

    /**
     * Creates new texture.
     *
     * @param key
     * @param lossless If set false, texture will be compressed in lossy format. \n
     *                 Set true if data integrity is necessary.
     * @return invalid handle if key is duplicated.
     */
    texture_handle create_texture(std::string key, bool lossless = false);

    /**
     * Creates new window.
     *
     * @param key
     * @return invalid handle if key is duplicated.
     */
    window_handle create_window(std::string key);

    /**
     * Creates new mesh
     *
     * @param
     * @return invalid handle if key is duplicated.
     */
    mesh_handle create_mesh(std::string key);

   public:
    /**
     * Upload texture. Update texture will be deferred until remote client request or,
     *  any dependency occurrence.
     *
     * @param handle
     * @param size buffer size. Size can vary, however, frequent size change may cause \n
     *              performance hit.
     * @param format pixel format.
     * @param base_buffer Initial data to copy into created texture. Once specified,
     *                     clear() function call with no arguments to dc will redraw
     *                     this buffer to render target. \n
     *                    Total data length is calculated from 'size' and 'format'.
     * @param draw_fn Generates draw command for given texture(render target). Once
     *                 called, previous draw commands which were not flushed yet will be
     *                 purged.
     */
    void draw(
            texture_handle handle,
            size2 size, pixel_format format,
            void const*            base_buffer = nullptr,
            texture_draw_fn const& draw_fn = perfkit::default_function);

    /**
     * Uploads texture only if possible. It'll not store any data into storage unless there's
     *  active dependency or request. Only its metadata(size and format) will be retained in
     *  the storage and remote client's context. \n
     * Due to the way this works, remote client will experience black or garbage texture for
     *  first few frames, or forever if try_draw would not be called again. \n
     *
     * @param handle
     * @param size
     * @param format
     * @param base_buffer
     * @param lossless
     * @param draw_fn
     * @return
     */
    bool try_draw(
            texture_handle handle,
            size2 size, pixel_format format,
            void const*            base_buffer = nullptr,
            texture_draw_fn const& draw_fn = perfkit::default_function);

    /**
     * Show window once. Size of the window will be determined by remote system. \n
     * From successful call to this function, window data is always cached in server storage,
     *  thus it can transfer window data as fast as possible on user's subscribe request.
     *
     * @param handle
     * @param wnd_proc
     */
    void show(
            window_handle         handle,
            window_proc_fn const& wnd_proc);

    /**
     * Show window only if possible. User will see empty window until you call this function
     *  again and corresponding data is transferred to the remote client.
     *
     * @param handle
     * @param wnd_proc
     */
    bool try_show(
            window_handle         handle,
            window_proc_fn const& wnd_proc);

    /**
     * Modal window until user closes the window. \n
     * This function never return control unless user closes modal, or modal_expired
     *  exception is thrown during iteration.
     *
     * @param handle
     * @param type
     * @param wnd_proc
     */
    modal_result
    modal(window_handle         handle,
          modal_type            type,
          window_proc_fn const& wnd_proc);

   public:
    /**
     * Start graphics update session
     */
    void _bk_start_session();

    /**
     * Stop graphics update session
     */
    void _bk_stop_session();

    /**
     * Reset all internal update/cache/subscription context. \n
     * Next call to \ref _bk_retrieve_updates will return overall information
     */
    void _bk_reset_caches();

    /**
     * Retrive updated draw call binaries. Internally swaps vector reference. (not copy) \n
     * Blocks connection until any valid update issued or _stop_session is called.
     */
    void _bk_retrieve_updates(std::string* out);

    /**
     * Change subscription state of given resource.
     */
    void _bk_change_subscription_state(handle_data const& key, bool new_state);

    /**
     * Commit event structure. \n
     * Internally swaps reference
     */
    void _bk_commit_events(std::string* in);

   private:
    class impl;
    std::unique_ptr<impl> self;
};
}  // namespace perfkit::graphics
