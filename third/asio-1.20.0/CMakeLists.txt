project(asio-bundled)

add_library(${PROJECT_NAME} INTERFACE)
add_library(perfkit::net::bundled-asio ALIAS ${PROJECT_NAME})

target_include_directories(
        ${PROJECT_NAME}

        INTERFACE
        include
)
