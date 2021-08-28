cmake_minimum_required(VERSION 3.20)

set(TARGET_CONTEXT_NAME)
add_executable(perfkit-automated-test automation.cpp automation-perfkit.cpp)
target_link_libraries(perfkit-automated-test PRIVATE perfkit::perfkit-static)

add_executable(example-tui example-tui.cpp)
target_link_libraries(example-tui PRIVATE perfkit::perfkit-static)

add_executable(curses-practice curses-practice.cpp)
target_link_libraries(curses-practice PRIVATE ${CURSES_NCURSES_LIBRARY})
target_include_directories(curses-practice PRIVATE ${CURSES_NCURSES_INCLUDE_DIR})
