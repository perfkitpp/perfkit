cmake_minimum_required(VERSION 3.20)
project(perfkit-curses)

find_package(Curses REQUIRED)
add_library(
        ${PROJECT_NAME} STATIC
        src/curses.cpp
)

add_library(
        perfkit::curses ALIAS ${PROJECT_NAME}
)

target_link_libraries(
        ${PROJECT_NAME}
        PUBLIC ${CURSES_NCURSES_LIBRARY}
        PUBLIC perfkit::core
)

target_include_directories(
        ${PROJECT_NAME}
        PUBLIC ${CURSES_INCLUDE_DIR}
)

