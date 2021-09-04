cmake_minimum_required(VERSION 3.20)
project(perfkit)

# Imports --------------------------------------------------------------------------------------------------------------
add_subdirectory(range-v3-master EXCLUDE_FROM_ALL)

# Module ---------------------------------------------------------------------------------------------------------------
file(GLOB_RECURSE ${PROJECT_NAME}_SOURCES "src/*")
file(GLOB_RECURSE ${PROJECT_NAME}_HEADERS "include/*")

add_library(${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SOURCES} ${${PROJECT_NAME}_HEADERS})
add_library(perfkit::perfkit-static ALIAS ${PROJECT_NAME})

# TUI integration
option(PERFKIT_ENABLE_UI "" ON)
option(PERFKIT_UI_USE_IMTUI "" ON)

add_subdirectory(ui-bundled)

if (PERFKIT_ENABLE_UI AND PERFKIT_UI_USE_IMTUI)
    target_link_libraries(
            ${PROJECT_NAME} PUBLIC
            imtui-bundled
    )
endif ()

# Properties -----------------------------------------------------------------------------------------------------------
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_definitions(${PROJECT_NAME}
        PUBLIC
        -DKANGSW_ARRAY_VIEW_NAMESPACE=perfkit
        -DSPDLOG_COMPILED_LIB=1)
target_link_libraries(${PROJECT_NAME} PUBLIC range-v3::range-v3)

if (UNIX)
    target_link_libraries(${PROJECT_NAME} PUBLIC pthread)
endif ()

# Example Directory ----------------------------------------------------------------------------------------------------
option(perfkit_BUILD_EXAMPLES "" ON)
if (perfkit_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

