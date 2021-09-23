cmake_minimum_required(VERSION 3.20)
project(perfkit)

# Imports --------------------------------------------------------------------------------------------------------------
add_subdirectory(range-v3-master EXCLUDE_FROM_ALL)

# Module ---------------------------------------------------------------------------------------------------------------
set(
        PERFKIT_CORE_SOURCES
        "src/command_registry.cpp"
        "src/configs.cpp"
        "src/main.cpp"
        "src/perfkit.cpp"
        "src/tracer.cpp"
)

add_library(${PROJECT_NAME} STATIC ${PERFKIT_CORE_SOURCES})
add_library(perfkit::core ALIAS ${PROJECT_NAME})

# Configurations -------------------------------------------------------------------------------------------------------
option(perfkit_BUILD_FTXUI_EXTENSION "" OFF)
option(perfkit_BUILD_NET_CLIENT "" ON)
option(perfkit_USE_BUNDLED_JSON "" ON)

# JSON Bundling
if (perfkit_USE_BUNDLED_JSON)
    message("Using bundled json")
    target_include_directories(${PROJECT_NAME} PUBLIC "extensions/json")
endif ()

# TUI Extension
if (perfkit_BUILD_FTXUI_EXTENSION)
    message("Configuring FTXUI Extension Library ...")
    add_subdirectory(extensions/ftxui)
endif ()

# NET Extension
if (perfkit_BUILD_NET_CLIENT)
    message("Configuring NET CLIENT extension ...")
    add_subdirectory(extensions/net-client)
endif ()

# Properties -----------------------------------------------------------------------------------------------------------
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

if (NOT TARGET spdlog)
    find_package(spdlog REQUIRED)
endif ()

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_definitions(${PROJECT_NAME}
                           PUBLIC
                           -DKANGSW_ARRAY_VIEW_NAMESPACE=perfkit
                           -DSPDLOG_COMPILED_LIB=1)
target_link_libraries(${PROJECT_NAME} PUBLIC range-v3::range-v3 spdlog::spdlog)

if (UNIX)
    target_link_libraries(${PROJECT_NAME} PUBLIC pthread)
endif ()

# Example Directory ----------------------------------------------------------------------------------------------------
option(perfkit_BUILD_EXAMPLES "" ON)
if (perfkit_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

