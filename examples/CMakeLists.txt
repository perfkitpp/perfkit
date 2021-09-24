cmake_minimum_required(VERSION 3.16)

set(TARGET_CONTEXT_NAME)
add_executable(perfkit-automated-test automation.cpp automation-perfkit.cpp)
target_link_libraries(perfkit-automated-test PRIVATE perfkit::core range-v3::range-v3)

if (perfkit_BUILD_FTXUI_EXTENSION)
    add_executable(example-tui example-tui.cpp)
    target_link_libraries(example-tui PRIVATE perfkit::ftxui range-v3::range-v3)

    add_executable(ex-ftxui-extension example-ftxui-extension.cpp)
    target_link_libraries(ex-ftxui-extension PRIVATE perfkit::ftxui range-v3::range-v3)
endif ()

#add_executable(curses-practice curses-practice.cpp)
#target_link_libraries(curses-practice PRIVATE ${CURSES_NCURSES_LIBRARY})
#target_include_directories(curses-practice PRIVATE ${CURSES_NCURSES_INCLUDE_DIR})
