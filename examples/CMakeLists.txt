set(TARGET_CONTEXT_NAME)

# ======================================================================================================================
project(perfkit-automated-test)

add_executable(
        ${PROJECT_NAME}
        automation-argparse.cpp
)

target_link_libraries(
        ${PROJECT_NAME}

        PRIVATE
        perfkit::core
        range-v3::range-v3
        doctest_with_main
)

# ======================================================================================================================
project(example-net)

add_executable(
        ${PROJECT_NAME}

        example-net.cpp
        test_class.cpp
)

target_link_libraries(
        ${PROJECT_NAME}

        PRIVATE
        perfkit::net-v2::core
)

# ======================================================================================================================
project(example-web)

add_executable(
        ${PROJECT_NAME}

        example-web.cpp
        test_class.cpp
)

target_link_libraries(
        ${PROJECT_NAME}

        PRIVATE
        perfkit::web
        perfkit::apptemplate
)

# ======================================================================================================================
project(apptemplate)

add_executable(
        ${PROJECT_NAME}

        apptemplate.cpp
)

target_link_libraries(
        ${PROJECT_NAME}

        PRIVATE
        perfkit::net-v2::core
        perfkit::apptemplate
)

