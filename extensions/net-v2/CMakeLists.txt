# ================================ TARGET: PROTOCOLS
project(perfkit-net-v2-protocol)

add_library(
        ${PROJECT_NAME}

        STATIC
        protocols/perfkit/extension/net/protocol.cpp
)

add_library(
        perfkit::net-v2::protocol ALIAS ${PROJECT_NAME}
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::core
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        protocols
)

# ================================ TARGET: NET CLIENT
project(perfkit-net-v2)

add_library(
        ${PROJECT_NAME}

        STATIC
        src/utils.cpp
        src/net_terminal.cpp
        src/net.cpp
)

add_library(
        perfkit::net-v2::core ALIAS ${PROJECT_NAME}
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::core

        PRIVATE
        perfkit::net-v2::protocol
        perfkit::asio
        perfkit::picosha2
        range-v3::range-v3
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include
)
