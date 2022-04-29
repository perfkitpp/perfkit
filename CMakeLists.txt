cmake_minimum_required(VERSION 3.16)
project(perfkit)


# Configurations -------------------------------------------------------------------------------------------------------
option(perfkit_BUILD_FTXUI_EXTENSION "" OFF)
option(perfkit_BUILD_NET "" ON)
option(perfkit_BUILD_CLI "" ON)
option(perfkit_BUILD_GRAPHICS "" ON)
option(perfkit_USE_BUNDLED_ASIO "" ON)
option(perfkit_BUILD_EXAMPLES "" OFF)
option(perfkit_BUILD_CPPHEADERS_TEST "" ON)

# Imports --------------------------------------------------------------------------------------------------------------
add_subdirectory(third)

# Modules --------------------------------------------------------------------------------------------------------------
add_subdirectory(modules)

# Example Directory ----------------------------------------------------------------------------------------------------
if (perfkit_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
