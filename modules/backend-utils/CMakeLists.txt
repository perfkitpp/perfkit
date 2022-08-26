project(perfkit-backend-utils)
add_library(${PROJECT_NAME} STATIC)
add_library(perfkit::internal::backend-utils ALIAS ${PROJECT_NAME})

target_sources(
        ${PROJECT_NAME} PRIVATE

        backend-utils.cpp
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC include
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC perfkit::core
)
