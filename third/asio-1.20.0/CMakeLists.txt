option(perfkit_ASIO_HEADERONLY ON)

## ASIO HEADER ONLY
project(asio-header-only)

add_library(${PROJECT_NAME} INTERFACE)
add_library(perfkit::asio-header-only ALIAS ${PROJECT_NAME})

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

## ASIO STATIC
project(asio-static)

add_library(
        ${PROJECT_NAME}

        STATIC
        src/asio.cpp
)

add_library(
        perfkit::asio-static ALIAS ${PROJECT_NAME}
)

target_compile_definitions(
        ${PROJECT_NAME}

        PUBLIC
        ASIO_SEPARATE_COMPILATION=1
)

target_include_directories(
        ${PROJECT_NAME}

        PUBLIC
        include
)

target_compile_features(
        ${PROJECT_NAME}

        PUBLIC
        cxx_std_17
)

# Configure OpenSSl
find_package(OpenSSL)

if (OpenSSL_FOUND)
    target_sources(
            ${PROJECT_NAME}

            PUBLIC
            src/asio_ssl.cpp
    )

    target_link_libraries(
            ${PROJECT_NAME}

            PUBLIC
            OpenSSL::SSL
    )
endif ()

if (MSVC)
    target_compile_definitions(
            ${PROJECT_NAME}

            PUBLIC
            _WIN32_WINNT=0x0601
    )
endif ()

## H/O or ..
if (perfkit_ASIO_HEADERONLY)
    add_library(perfkit::asio ALIAS asio-header-only)
else()
    add_library(perfkit::asio ALIAS asio-static)
endif ()
