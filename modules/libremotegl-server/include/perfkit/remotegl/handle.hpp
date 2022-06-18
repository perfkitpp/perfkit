#pragma once
#include "cpph/utility/hasher.hxx"

namespace perfkit {
using namespace cpph;
}

namespace perfkit::rgl {
using texture_handle = basic_key<class RemoteGL_TextureLabel>;
using render_target_handle = basic_key<class RemoteGL_RenderTargetLabel>;
using window_handle = basic_key<class RemoteGL_WindowHandleLabel>;
using mesh_handle = basic_key<class RemoteGL_MeshHandleLabel>;
using material_handle = basic_key<class RemoteGL_MaterialHandleLabel>;

}  // namespace perfkit::rgl
