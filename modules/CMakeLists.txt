# Module ---------------------------------------------------------------------------------------------------------------
add_subdirectory(core)

# TUI Extension
if (perfkit_BUILD_FTXUI_EXTENSION)
    if (NOT TARGET ftxui::dom)
        message(FATAL_ERROR "FTXUI not found!")
    endif ()

    message("[${PROJECT_NAME}]: Configuring FTXUI Extension Library ...")
    add_subdirectory(ftxui)
endif ()

if (perfkit_BUILD_NET OR perfkit_BUILD_GRAPHICS)
    message("[${PROJECT_NAME}]: Configuring graphics extension ... ")
    add_subdirectory(libremotegl-server)
    add_subdirectory(libremotegl-protocol)
endif ()

if (perfkit_BUILD_NET)
    message("[${PROJECT_NAME}]: Configuring NET extension ...")
    add_subdirectory(net-v2)
endif ()

if (perfkit_BUILD_CLI)
    message("[${PROJECT_NAME}]: Configuring CLI extension ...")
    add_subdirectory(cli)

    if (UNIX)
        target_sources(
                perfkit-cli

                PRIVATE
                ../third/linenoise-1.0/linenoise.c
                ../third/linenoise-1.0/linenoise.h
        )
    endif ()

    target_include_directories(
            perfkit-cli

            PRIVATE
            ../third/linenoise-1.0
    )
endif ()

# TARGET [apptemplate] -------------------------------------------------------------------------------------------------
add_subdirectory(apptemplate)

# Properties -----------------------------------------------------------------------------------------------------------
