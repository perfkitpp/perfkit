project(perfkit-net)

# ================================ TARGET: NET CLIENT
add_library(
        ${PROJECT_NAME}
        STATIC

        src/net.cpp
        src/net-context.cpp
        src/utils.cpp
        src/server_mode_dispatcher.cpp
        src/dispatcher.cpp
        src/net-terminal.cpp
        src/basic_dispatcher_impl.cpp

        src/contexts/config_watcher.cpp
        src/contexts/trace_watcher.cpp
        src/contexts/graphics_watcher.cpp
)

add_library(
        perfkit::net::core ALIAS ${PROJECT_NAME}
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::core

        PRIVATE
        perfkit::asio
        perfkit::picosha2
        range-v3::range-v3
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include
)
