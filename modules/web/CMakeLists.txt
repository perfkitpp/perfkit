project(perfkit-web)
add_library(${PROJECT_NAME} STATIC)
add_library(perfkit::web ALIAS ${PROJECT_NAME})

target_sources(
        ${PROJECT_NAME} PRIVATE

        src/web.cpp
        src/web_terminal.cpp
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC perfkit::core

        PRIVATE perfkit::asio
        PRIVATE Crow::Crow
        PRIVATE perfkit::internal::backend-utils
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC include
)

install(
        DIRECTORY public/
        DESTINATION perfkit.web.public
)
