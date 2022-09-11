cmake_minimum_required(VERSION 3.16)
project(perfkit)


# Configurations -------------------------------------------------------------------------------------------------------
option(perfkit_BUILD_FTXUI_EXTENSION "" OFF)
option(perfkit_BUILD_NET "" ON)
option(perfkit_BUILD_CLI "" OFF)
option(perfkit_BUILD_GRAPHICS "" ON)
option(perfkit_BUILD_WEB "" ON)
option(perfkit_USE_BUNDLED_ASIO "" ON)
option(perfkit_BUILD_EXAMPLES "" OFF)
option(perfkit_BUILD_CPPHEADERS_TEST "" ON)

set(perfkit_CROW_DIR_OVERRIDE "" CACHE PATH "Override source directory of CrowCpp")

# Imports --------------------------------------------------------------------------------------------------------------
add_subdirectory(third)

# Modules --------------------------------------------------------------------------------------------------------------
add_subdirectory(modules)

# Example Directory ----------------------------------------------------------------------------------------------------
if (perfkit_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
