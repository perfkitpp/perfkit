project(perfkit-web VERSION 0.1)
add_library(${PROJECT_NAME} STATIC)
add_library(perfkit::web ALIAS ${PROJECT_NAME})

target_sources(
        ${PROJECT_NAME} PRIVATE

        src/web.cpp
        src/web_terminal.cpp
        src/interface.cpp
        src/config_server.cpp
        src/trace_server.cpp
        src/graphic_server.cpp
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC perfkit::core

        PRIVATE perfkit::asio
        PRIVATE Crow::Crow
        PRIVATE perfkit::internal::backend-utils
        PRIVATE kang-sw::cpph::stb
)

target_compile_definitions(
        ${PROJECT_NAME}

        PRIVATE PERFKIT_WEB_INTERFACE_VERSION="v${PROJECT_VERSION}"
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC include
)

set(PERFKIT_WEB_PUBLIC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/public/" CACHE INTERNAL "")
set(PERFKIT_WEB_DEFAULT_DIRNAME "perfkit.web.public" CACHE INTERNAL "")

install(
        DIRECTORY ${PERFKIT_WEB_PUBLIC_FILES}/
        DESTINATION ${PERFKIT_WEB_DEFAULT_DIRNAME}
)
