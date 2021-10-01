project(perfkit-provider-protocols)

add_library(
        ${PROJECT_NAME} INTERFACE
)

add_library(
        perfkit::_internal::net-provider-protocols ALIAS ${PROJECT_NAME}
)

target_include_directories(
        ${PROJECT_NAME}

        INTERFACE
        .
)