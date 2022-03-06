cmake_minimum_required(VERSION 3.16)
project(perfkit)

# Imports --------------------------------------------------------------------------------------------------------------
add_subdirectory(third)

if (NOT TARGET range-v3::range-v3)
    message("[${PROJECT_NAME}]: Using bundled range-v3 library ...")
    add_subdirectory(third/range-v3-master EXCLUDE_FROM_ALL)
endif ()

# Module ---------------------------------------------------------------------------------------------------------------
set(
        PERFKIT_CORE_SOURCES

        src/commands.cpp
        src/configs.cpp
        src/main.cpp
        src/perfkit.cpp
        src/tracer.cpp
        src/terminal.cpp
        src/config-flags.cpp
        src/logging.cpp
)

add_library(${PROJECT_NAME} STATIC ${PERFKIT_CORE_SOURCES})
add_library(perfkit::core ALIAS ${PROJECT_NAME})

# Configurations -------------------------------------------------------------------------------------------------------
option(perfkit_BUILD_FTXUI_EXTENSION "" OFF)
option(perfkit_BUILD_NET_CLIENT "" ON)
option(perfkit_BUILD_NET "" ON)
option(perfkit_BUILD_CLI "" ON)
option(perfkit_BUILD_GRAPHICS "" ON)
option(perfkit_USE_BUNDLED_JSON "" ON)
option(perfkit_USE_BUNDLED_ASIO "" ON)
option(perfkit_BUILD_EXAMPLES "" OFF)
option(perfkit_BUILD_CPPHEADERS_TEST "" ON)

# JSON Bundling
if (perfkit_USE_BUNDLED_JSON)
    message("[${PROJECT_NAME}]: Using bundled json")
    target_include_directories(${PROJECT_NAME} PUBLIC "third/json")
else ()
    target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann::json)
endif ()

# TUI Extension
if (perfkit_BUILD_FTXUI_EXTENSION)
    if (NOT TARGET ftxui::dom)
        message(FATAL_ERROR "FTXUI not found!")
    endif ()

    message("[${PROJECT_NAME}]: Configuring FTXUI Extension Library ...")
    add_subdirectory(extensions/ftxui/)
endif ()

# NET Extension
if (perfkit_USE_BUNDLED_ASIO OR perfkit_BUILD_NET_CLIENT)
    message("[${PROJECT_NAME}]: Using bundled asio library ... ")
    add_subdirectory(third/asio-1.20.0)
endif ()

# NET depends on graphics module
if (perfkit_BUILD_NET OR perfkit_BUILD_GRAPHICS)
    message("[${PROJECT_NAME}]: Configuring graphics extension ... ")
    add_subdirectory(extensions/graphics)
endif ()

if (perfkit_BUILD_NET_CLIENT OR perfkit_BUILD_NET)
    message("[${PROJECT_NAME}]: Configuring NET extension ...")
    add_subdirectory(extensions/net)
    add_subdirectory(extensions/net-v2)
endif ()

if (perfkit_BUILD_CLI)
    message("[${PROJECT_NAME}]: Configuring CLI extension ...")
    add_subdirectory(extensions/cli)

    if (UNIX)
        target_sources(
                perfkit-cli

                PRIVATE
                third/linenoise-1.0/linenoise.c
                third/linenoise-1.0/linenoise.h
        )
    endif ()

    target_include_directories(
            perfkit-cli

            PRIVATE
            third/linenoise-1.0
    )
endif ()

# Properties -----------------------------------------------------------------------------------------------------------
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

if (NOT TARGET spdlog)
    find_package(spdlog CONFIG REQUIRED)
endif ()

find_library(LIBDW dw)
if (LIBDW)
    message("[${PROJECT_NAME}]: LIBDW FOUND, enabling rich backtrace support ...")
    target_link_libraries(
            ${PROJECT_NAME}

            PRIVATE
            dw
    )
    target_compile_definitions(
            ${PROJECT_NAME}

            PUBLIC
            -DBACKWARD_HAS_DW=1
    )
endif ()

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include/

        PRIVATE
        third/linenoise-1.0/
)

target_compile_features(
        ${PROJECT_NAME}

        PUBLIC
        cxx_std_17
)

target_compile_definitions(
        ${PROJECT_NAME}

        PUBLIC
        -DKANGSW_TEMPLATE_NAMESPACE=perfkit
        -DSPDLOG_COMPILED_LIB=1
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        spdlog::spdlog
        perfkit::cpph

        PRIVATE
        range-v3::range-v3
)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    message("[${PROJECT_NAME}]: enabling trace log level ...")
    target_compile_definitions(
            ${PROJECT_NAME}

            PUBLIC
            -DSPDLOG_ACTIVE_LEVEL=0
    )
endif ()


if (UNIX)
    target_link_libraries(
            ${PROJECT_NAME}

            PUBLIC
            pthread
    )
endif ()

# Example Directory ----------------------------------------------------------------------------------------------------
if (perfkit_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (MSVC)
    target_compile_options(
            ${PROJECT_NAME}

            PUBLIC
            /utf-8 /FIciso646
    )

    add_compile_options(/FIciso646)
endif ()

if (perfkit_BUILD_CPPHEADERS_TEST)
    message("[${PROJECT_NAME}]: Configuring imported cppheaders tests ...")
    add_subdirectory(third/cpph/include/perfkit/common)
endif ()
