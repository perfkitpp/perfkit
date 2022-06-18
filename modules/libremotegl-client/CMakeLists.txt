project(libremotegl-client)
add_library(${PROJECT_NAME} STATIC)
add_library(perfkit::remotegl::client ALIAS ${PROJECT_NAME})

target_sources(
        ${PROJECT_NAME}

        PRIVATE
        src/remotegl.cpp
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::remotegl::protocol
)

