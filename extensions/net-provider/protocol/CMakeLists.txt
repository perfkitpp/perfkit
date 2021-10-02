project(perfkit-provider-protocols)

add_library(
        ${PROJECT_NAME}
        STATIC

        src/net-proto.cpp
)

add_library(
        perfkit::_internal::net-provider-protocols ALIAS ${PROJECT_NAME}
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include/
)

target_link_libraries(
        ${PROJECT_NAME}

        PUBLIC
        perfkit::core
)
