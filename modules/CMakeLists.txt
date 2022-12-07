# Module ---------------------------------------------------------------------------------------------------------------
add_subdirectory(core)
add_subdirectory(backend-utils)

# TUI Extension
if (perfkit_BUILD_FTXUI_EXTENSION)
    if (NOT TARGET ftxui::dom)
        message(FATAL_ERROR "FTXUI not found!")
    endif ()

    message("[${PROJECT_NAME}]: Configuring FTXUI Extension Library ...")
    add_subdirectory(ftxui)
endif ()

if (perfkit_BUILD_NET)
    message("[${PROJECT_NAME}]: Configuring NET extension ...")
    add_subdirectory(net-v2)
endif ()

if (perfkit_BUILD_WEB)
    message("[${PROJECT_NAME}]: Configuring WEB extension ...")
    add_subdirectory(web)
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
add_subdirectory(mongo-config)

# Properties -----------------------------------------------------------------------------------------------------------
