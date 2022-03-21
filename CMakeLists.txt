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

# Imports --------------------------------------------------------------------------------------------------------------
if (NOT TARGET range-v3::range-v3)
    message("[${PROJECT_NAME}]: Using bundled range-v3 library ...")
    add_subdirectory(third/range-v3-master EXCLUDE_FROM_ALL)
endif ()

if (NOT TARGET nlohmann::json)
    message("[${PROJECT_NAME}]: Using bundled nlohmann::json library ...")

    add_library(json INTERFACE)
    add_library(nlohmann::json ALIAS json)
    target_include_directories(json INTERFACE "third/json")
endif ()

if (NOT TARGET spdlog)
    find_package(spdlog CONFIG REQUIRED)
endif ()

if (perfkit_USE_BUNDLED_ASIO OR perfkit_BUILD_NET_CLIENT)
    message("[${PROJECT_NAME}]: Using bundled asio library ... ")
    add_subdirectory(third/asio-1.20.0)
endif ()

# Modules --------------------------------------------------------------------------------------------------------------
add_subdirectory(modules)

# Example Directory ----------------------------------------------------------------------------------------------------
if (perfkit_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (perfkit_BUILD_CPPHEADERS_TEST)
    message("[${PROJECT_NAME}]: Configuring imported cppheaders tests ...")
    add_subdirectory(third/cpph/include/perfkit/common)
endif ()
