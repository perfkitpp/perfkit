project(perfkit-net-provider)

add_library(
        ${PROJECT_NAME}
        STATIC

        "src/net-console.cpp"
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::core
)
