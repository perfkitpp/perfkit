project(perfkit-web VERSION 0.1)
add_library(${PROJECT_NAME} STATIC)
add_library(perfkit::web ALIAS ${PROJECT_NAME})

target_sources(
        ${PROJECT_NAME} PRIVATE

        src/web.cpp
        src/web_terminal.cpp
        src/interface.cpp
        src/config_server.cpp
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC perfkit::core

        PRIVATE perfkit::asio
        PRIVATE Crow::Crow
        PRIVATE perfkit::internal::backend-utils
)

target_compile_definitions(
        ${PROJECT_NAME}

        PRIVATE PERFKIT_WEB_INTERFACE_VERSION="v${PROJECT_VERSION}"
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC include
)

install(
        DIRECTORY public/
        DESTINATION perfkit.web.public/v${PROJECT_VERSION}
)
