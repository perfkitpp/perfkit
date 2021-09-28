set(TARGET_CONTEXT_NAME)

# ======================================================================================================================
add_executable(
        perfkit-automated-test

        automation.cpp
        automation-perfkit.cpp
        automation-argparse.cpp
)

target_link_libraries(
        perfkit-automated-test

        PRIVATE
        perfkit::core
        range-v3::range-v3
)

# ======================================================================================================================
add_executable(
        example-cli

        example-cli.cpp
)

target_link_libraries(
        example-cli

        PRIVATE
        perfkit::cli
)

# ======================================================================================================================
if (perfkit_BUILD_FTXUI_EXTENSION)

    # ex-ftxui-extension
    add_executable(
            ex-ftxui-extension

            example-ftxui-extension.cpp
    )

    target_link_libraries(
            ex-ftxui-extension

            PRIVATE
            perfkit::ftxui
            range-v3::range-v3
    )

endif ()

#add_executable(curses-practice curses-practice.cpp)
#target_link_libraries(curses-practice PRIVATE ${CURSES_NCURSES_LIBRARY})
#target_include_directories(curses-practice PRIVATE ${CURSES_NCURSES_INCLUDE_DIR})
