project(libremotegl-server)
add_library(${PROJECT_NAME} STATIC)
add_library(perfkit::remotegl::server ALIAS ${PROJECT_NAME})

target_sources(
        ${PROJECT_NAME}

        PRIVATE
        src/remotegl.cpp
        src/context.cpp

        src/resource/texture.cpp
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

