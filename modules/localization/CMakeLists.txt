project(perfkit.localizatioon)
add_library(${PROJECT_NAME} STATIC)
add_library(perfkit::localization ALIAS ${PROJECT_NAME})

target_sources(
        ${PROJECT_NAME}

        PRIVATE
        src/localize.cpp
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::cpph
)
