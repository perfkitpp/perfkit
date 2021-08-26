cmake_minimum_required(VERSION 3.20)
project(perfkit)

# Imports --------------------------------------------------------------------------------------------------------------

# Options --------------------------------------------------------------------------------------------------------------
option(${PROJECT_NAME}_BUILD_SHARED "Build shared version of library" OFF)

# Module ---------------------------------------------------------------------------------------------------------------
file(GLOB_RECURSE ${PROJECT_NAME}_SOURCES "src/*")
file(GLOB_RECURSE ${PROJECT_NAME}_HEADERS "include/*")

if (${PROJECT_NAME}_BUILD_SHARED)
    add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SOURCES} ${${PROJECT_NAME}_HEADERS})
    add_library(perfkit::perfkit ALIAS ${PROJECT_NAME})
else ()
    add_library(${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SOURCES} ${${PROJECT_NAME}_HEADERS})
    add_library(perfkit::perfkit-static ALIAS ${PROJECT_NAME})
endif ()

# TUI integration
find_package(Curses)
if (Curses_FOUND)
    file(GLOB_RECURSE ${PROJECT_NAME}_TUI_SOURCES "tui/*")

    message("Found Curses. Creating imtui target ...")
    target_sources(${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_TUI_SOURCES})

    target_link_libraries(${PROJECT_NAME} PRIVATE ${CURSES_NCURSES_LIBRARY})
    target_include_directories(${PROJECT_NAME} PRIVATE ${CURSES_INCLUDE_DIR})
endif ()

# Properties -----------------------------------------------------------------------------------------------------------
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

if (UNIX)
    target_link_libraries(${PROJECT_NAME} PUBLIC pthread)
endif ()

# Example Directory ----------------------------------------------------------------------------------------------------
add_subdirectory(examples)
