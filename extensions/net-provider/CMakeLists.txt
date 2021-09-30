project(perfkit-net-provider)

# ================================ TARGET: PROTOCOLS
add_subdirectory(protocol)

# ================================ TARGET: NET CLIENT
add_library(
        ${PROJECT_NAME}
        STATIC

        "src/net-provider.cpp"
        "include/perfkit/extension/net-provider.hpp"
        src/net_terminal.cpp src/net_terminal.hpp)

add_library(
        perfkit::net-provider ALIAS ${PROJECT_NAME}
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::core

        PRIVATE
        perfkit::_internal::net-provider-protocols
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include
)
