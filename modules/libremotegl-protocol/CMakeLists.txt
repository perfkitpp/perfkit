project(libremotegl-protocol)
add_library(${PROJECT_NAME} INTERFACE)
add_library(perfkit::remotegl::protocol ALIAS ${PROJECT_NAME})

target_include_directories(
        ${PROJECT_NAME}

        INTERFACE
        include
)

target_link_libraries(
        ${PROJECT_NAME}

        INTERFACE
        perfkit::cpph
)

target_sources(
        ${PROJECT_NAME}

        INTERFACE
        src/bodies.cpp
)

