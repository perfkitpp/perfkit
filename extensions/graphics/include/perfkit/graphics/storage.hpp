/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2021. Seungwoo Kang
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
     * @return invalid handle if key is duplicated.
     */
    texture_handle create_texture(std::string key);

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
     * Request texture upload
     *
     * @param handle
     * @param size
     * @param buffer
     * @param buflen
     * @param lossless
     */
    void upload(
            texture_handle handle,
            size2 size, pixel_format format,
            void const* buffer = nullptr,
            bool lossless      = false);

    /**
     * Upload texture only if possible.
     *
     * @param handle
     * @param size
     * @param format
     * @param buffer
     * @param lossless
     * @return
     */
    bool try_upload(
            texture_handle handle,
            size2 size, pixel_format format,
            void const* buffer = nullptr,
            bool lossless      = false);

    /**
     * Show window once.\n
     * Size of the window will be determined by remote system.
     *
     * @param handle
     * @param wnd_proc
     */
    void show(
            window_handle handle,
            window_proc_fn wnd_proc);

    /**
     * Show window only if possible.
     *
     * @param handle
     * @param wnd_proc
     */
    void try_show(
            window_handle handle,
            window_proc_fn wnd_proc);

    /**
     * Modal window until user closes the window. \n
     * If
     *
     * @param handle
     * @param type
     * @param wnd_proc
     */
    void modal(
            window_handle handle,
            modal_type type,
            window_proc_fn wnd_proc);

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
     * Reset all internal update/cache context. \n
     * Next call to \ref _bk_retrieve_updates will return overall information
     */
    void _bk_reset_caches();

    /**
     * Retrive updated draw call binaries.
     */
    void _bk_retrieve_updates(std::vector<char>* out);

    /**
     * Change subscription state of given resource.
     */
    void _bk_change_subscription_state(handle_data const& key, bool new_state);

   private:
    class impl;
    std::unique_ptr<storage> self;
};
}  // namespace perfkit::graphics
