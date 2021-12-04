project(asio-bundled)

add_library(${PROJECT_NAME} INTERFACE)
add_library(perfkit::net::asio ALIAS ${PROJECT_NAME})

if (NOT TARGET asio::standalone)
    add_library(asio::standalone ALIAS ${PROJECT_NAME})
endif ()

target_include_directories(
        ${PROJECT_NAME}

        INTERFACE
        include
)

target_compile_features(
        ${PROJECT_NAME}

        INTERFACE
        cxx_std_17
)
