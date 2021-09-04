cmake_minimum_required(VERSION 3.20)
project(imgui-bundled)

add_library(
        ${PROJECT_NAME} STATIC
        ./imgui.cpp
        ./imgui_draw.cpp
        ./imgui_tables.cpp
        ./imgui_widgets.cpp
)

target_include_directories(
        ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

set_property(
        TARGET imgui-bundled
        PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(
        ${PROJECT_NAME}-demo STATIC
        ./imgui_demo.cpp
)

target_link_libraries(
        ${PROJECT_NAME}-demo
        PUBLIC          imgui-bundled
)




