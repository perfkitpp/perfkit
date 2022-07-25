#pragma once
#include <cpph/std/string>

#include "basic_resource_handle.hpp"
#include "command.defs.hpp"
#include "cpph/helper/macros.hxx"

namespace perfkit::rgl {

PERFKITINTERNAL_RGL_SERVERCMD(resource_registered)
{
    CPPH_REFL_DECLARE_c;

    // Newly registered handle value
    basic_resource_handle new_handle = {};

    // Name of this resource.
    string alias;
};

PERFKITINTERNAL_RGL_SERVERCMD(resource_unregistered)
{
    CPPH_REFL_DECLARE_c;

    // Handle to close
    basic_resource_handle closed_handle = {};
};

}  // namespace perfkit::rgl