cmake_minimum_required(VERSION 3.20)
project(imtui-bundled)

add_library(
        ${PROJECT_NAME} STATIC
        src/imtui-impl-ncurses.cpp
        src/imtui-impl-text.cpp
)

target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE     ${CURSES_NCURSES_LIBRARY}
        PUBLIC      imgui-bundled
)

target_include_directories(
        ${PROJECT_NAME}
        PRIVATE     ${CURSES_INCLUDE_DIR}
)

target_include_directories(
        ${PROJECT_NAME}
        PUBLIC      ./include
)
