cmake_minimum_required(VERSION 3.20)
project(perfkit)

# Imports --------------------------------------------------------------------------------------------------------------
add_subdirectory(range-v3-master EXCLUDE_FROM_ALL)

# Options --------------------------------------------------------------------------------------------------------------
option(${PROJECT_NAME}_BUILD_SHARED "Build shared version of library" OFF)

# Module ---------------------------------------------------------------------------------------------------------------
file(GLOB_RECURSE ${PROJECT_NAME}_SOURCES "src/*")
file(GLOB_RECURSE ${PROJECT_NAME}_HEADERS "include/*")

if (${PROJECT_NAME}_BUILD_SHARED)
    add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SOURCES} ${${PROJECT_NAME}_HEADERS})
    add_library(perfkit::perfkit ALIAS ${PROJECT_NAME})
else ()
    add_library(${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SOURCES} ${${PROJECT_NAME}_HEADERS} include/perfkit/detail/command_registry.hpp src/command_registry.cpp include/Taywee/args.hxx ui/ui-tools.hpp ui/tui_dashboard/tui.cpp ui/tui_dashboard/tui.hpp src/perfkit.cpp)
    add_library(perfkit::perfkit-static ALIAS ${PROJECT_NAME})
endif ()

# TUI integration
find_package(Curses)
if (Curses_FOUND)
    file(GLOB_RECURSE ${PROJECT_NAME}_TUI_SOURCES "ui/*")

    message("Found Curses. Creating imtui target ...")
    target_sources(${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_TUI_SOURCES})

    target_link_libraries(${PROJECT_NAME} PRIVATE ${CURSES_NCURSES_LIBRARY})
    target_include_directories(${PROJECT_NAME} PRIVATE ${CURSES_INCLUDE_DIR})

    target_include_directories(${PROJECT_NAME} PRIVATE "ui")
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

