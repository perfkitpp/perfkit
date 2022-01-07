project(asio-bundled)

option(perfkit_ASIO_HEADERONLY OFF)

if (NOT TARGET asio::standalone)
    if (perfkit_ASIO_HEADERONLY)
        add_library(${PROJECT_NAME} INTERFACE)
        add_library(perfkit::net::asio ALIAS ${PROJECT_NAME})

        if (NOT TARGET asio::standalone)
            add_library(asio::standalone ALIAS ${PROJECT_NAME})
        endif ()

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
    else ()
        add_library(
                ${PROJECT_NAME}

                STATIC
                src/asio.cpp
        )

        add_library(
                perfkit::net::asio ALIAS ${PROJECT_NAME}
        )

        add_library(
                asio::standalone ALIAS ${PROJECT_NAME}
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

                    src/asio_ssl.cpp
            )

            target_link_libraries(
                    ${PROJECT_NAME}

                    PUBLIC
                    OpenSSL::SSL
            )
        endif ()
    endif ()
endif ()
