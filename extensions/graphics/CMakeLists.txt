project(perfkit-graphics)

add_library(
        ${PROJECT_NAME}

        STATIC
        src/storage.cpp
        src/api.cpp
)

add_library(
        perfkit::graphics

        ALIAS
        ${PROJECT_NAME}
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::core
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include

        PRIVATE
        src
)

