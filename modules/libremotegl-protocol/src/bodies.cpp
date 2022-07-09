#include "cpph/refl/object.hxx"
#include "perfkit/remotegl/protocol/command.defs.hpp"
#include "perfkit/remotegl/protocol/command.generic.hpp"
#include "perfkit/remotegl/protocol/command.tex.hpp"

namespace perfkit::rgl {
CPPH_REFL_DEFINE_PRIM_binary(basic_resource_handle);
CPPH_REFL_DEFINE_OBJECT(server_command<servercmd::resource_registered>, (), (new_handle), (alias));
CPPH_REFL_DEFINE_OBJECT(server_command<servercmd::resource_unregistered>, (), (closed_handle));
}  // namespace perfkit::rgl
