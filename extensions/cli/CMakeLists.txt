project(perfkit-cli)

add_library(
        ${PROJECT_NAME}
        STATIC

        include/perfkit/extension/cli.hpp
        src/basic_interactive_terminal.cpp
        src/basic_interactive_terminal.hpp
        src/cli.cpp
)

add_library(
        perfkit::cli
        ALIAS ${PROJECT_NAME}
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::core
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include
)
