project(core)

add_library(${PROJECT_NAME} STATIC)
add_library(perfkit::core ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_sources(
        ${PROJECT_NAME}

        PRIVATE
        src/commands.cpp
        src/configs.cpp
        src/main.cpp
        src/perfkit.cpp
        src/tracer.cpp
        src/terminal.cpp
        src/config-flags.cpp
        src/logging.cpp
)

target_precompile_headers(
        ${PROJECT_NAME}

        PUBLIC
        fix.hpp
)

if (WIN32)
    target_sources(
            ${PROJECT_NAME}

            PRIVATE
            src/terminal-windows.cpp
    )
else ()
    target_sources(
            ${PROJECT_NAME}

            PRIVATE
            src/terminal-linux.cpp
    )
endif ()


target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include/

        PRIVATE
        third/linenoise-1.0/
)

target_compile_features(
        ${PROJECT_NAME}

        PUBLIC
        cxx_std_17
)

target_compile_definitions(
        ${PROJECT_NAME}

        PUBLIC
        -DKANGSW_TEMPLATE_NAMESPACE=perfkit
        -DSPDLOG_COMPILED_LIB=1
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        spdlog::spdlog
        perfkit::cpph
        nlohmann_json::nlohmann_json

        PRIVATE
        range-v3::range-v3
)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    message("[${PROJECT_NAME}]: enabling trace log level ...")
    target_compile_definitions(
            ${PROJECT_NAME}

            PUBLIC
            -DSPDLOG_ACTIVE_LEVEL=0
    )
endif ()

if (UNIX)
    target_link_libraries(
            ${PROJECT_NAME}

            PUBLIC
            pthread
    )
endif ()

find_library(LIBDW dw)
if (LIBDW)
    message("[${PROJECT_NAME}]: LIBDW FOUND, enabling rich backtrace support ...")

    target_link_libraries(
            ${PROJECT_NAME}

            PRIVATE
            dw
    )

    target_compile_definitions(
            ${PROJECT_NAME}

            PUBLIC
            -DBACKWARD_HAS_DW=1
    )
endif ()

if (MSVC)
    target_compile_options(
            ${PROJECT_NAME}

            PUBLIC
            /utf-8 /FIciso646
    )

    add_compile_options(/FIciso646)
endif ()