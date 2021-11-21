project(perfkit-net-provider)

# ================================ TARGET: NET CLIENT
add_library(
        ${PROJECT_NAME}
        STATIC

        src/net-provider.cpp
        src/util.cpp
)

add_library(
        perfkit::net::core ALIAS ${PROJECT_NAME}
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::core

        PRIVATE
        perfkit::net::bundled-asio
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include
)
