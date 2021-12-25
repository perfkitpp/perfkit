project(perfkit-cpph)

add_library(
        ${PROJECT_NAME}

        INTERFACE
)

add_library(
        perfkit::cpph

        ALIAS
        ${PROJECT_NAME}
)

target_include_directories(
        ${PROJECT_NAME}

        INTERFACE
        include
)

target_compile_features(
        ${PROJECT_NAME}

        INTERFACE
        cxx_std_17
)

if (MSVC)
    target_compile_options(
            ${PROJECT_NAME}

            INTERFACE
            /utf-8
            /FIciso646
    )
endif ()