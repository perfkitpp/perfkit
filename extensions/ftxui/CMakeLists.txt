cmake_minimum_required(VERSION 3.16)
project(perfkit-ftxui)

option(perfkit_USE_BUNDLED_FTXUI "" ON)

if (perfkit_USE_BUNDLED_FTXUI)
    add_subdirectory(FTXUI EXCLUDE_FROM_ALL)
endif ()

add_library(
        ${PROJECT_NAME}
        STATIC

        "src/ftxui-extension.cpp"
        "src/config_browser.hpp"
        "src/trace_browser.hpp"
        "src/net-ftxui.cpp"
)

add_library(perfkit::ftxui ALIAS ${PROJECT_NAME})

target_link_libraries(
        ${PROJECT_NAME} PUBLIC

        PUBLIC
        ftxui::screen
        ftxui::dom
        ftxui::component
        perfkit::core

        PRIVATE
        range-v3::range-v3
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include
)