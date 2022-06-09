set(TARGET_CONTEXT_NAME)

# ======================================================================================================================
add_executable(
        perfkit-automated-test
        automation-argparse.cpp
)

target_link_libraries(
        perfkit-automated-test

        PRIVATE
        perfkit::core
        range-v3::range-v3
        doctest_with_main
)

# ======================================================================================================================
add_executable(
        example-net

        example-net.cpp
        test_class.cpp
)

target_link_libraries(
        example-net

        PRIVATE
        perfkit::net-v2::core
)
