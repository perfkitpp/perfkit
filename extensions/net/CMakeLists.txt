project(perfkit-net)

# ================================ TARGET: NET CLIENT
add_library(
        ${PROJECT_NAME}
        STATIC

        src/net-provider.cpp
        src/net-context.cpp
        src/utils.cpp
        src/server_mode_dispatcher.cpp
        src/dispatcher.cpp
        src/basic_dispatcher_impl.cpp
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
        perfkit::picosha2
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include
)
