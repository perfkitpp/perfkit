if (TARGET perfkit::third::imgui)
    return()
endif ()

project(perfkit-imported-imgui)
FetchContent_Declare(imgui URL https://github.com/ocornut/imgui/archive/refs/heads/docking.zip)
FetchContent_Populate(imgui)

add_library(${PROJECT_NAME} INTERFACE)
add_library(perfkit::third::imgui ALIAS ${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_11)

target_sources(
        ${PROJECT_NAME} INTERFACE

        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui.cpp
)

target_include_directories(
        ${PROJECT_NAME} INTERFACE

        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

function(PerfkitLinkImGuiGlfw TargetName)
    if (NOT TARGET glfw)
        find_package(glfw3 CONFIG QUIET)
        if (NOT TARGET glfw)
            message(WARNING "[${PROJECT_NAME}] Could not found ImGui ... Fetching one!")
            FetchContent_Declare(glfw URL https://github.com/glfw/glfw/archive/refs/tags/3.3.7.tar.gz)
            FetchContent_Populate(glfw)
            add_subdirectory(${glfw_SOURCE_DIR} ${glfw_BINARY_DIR} EXCLUDE_FROM_ALL)
        endif ()
    endif ()

    message(STATUS "[perfkit] Configuring opengl3 backend to [${TargetName}] ...")
    find_package(OpenGL REQUIRED COMPONENTS OpenGL)

    target_sources(
            ${TargetName} PRIVATE
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_include_directories(
            ${TargetName} PUBLIC
            ${OpenGL_INCLUDE_DIRS}
    )

    target_link_libraries(
            ${TargetName} PUBLIC glfw ${OPENGL_LIBRARIES}
    )
endfunction()
