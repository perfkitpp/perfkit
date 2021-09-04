
if (PERFKIT_ENABLE_UI)
    message("UI features enaled. Configuring IMGUI ...")
    add_subdirectory(imgui)
endif ()

if (PERFKIT_ENABLE_UI AND PERFKIT_UI_USE_IMTUI)
    message("IMTUI has selected as IMGUI backend. Configuring ...")
    find_package(Curses REQUIRED)
    add_subdirectory(imtui)
endif ()