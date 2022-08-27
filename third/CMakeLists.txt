include(FetchContent)

### CONFIGURE PICOSHA2
add_library(
        PicoSHA2 INTERFACE
)

add_library(
        perfkit::picosha2 ALIAS PicoSHA2
)

target_include_directories(
        PicoSHA2 INTERFACE PicoSHA2/
)

### CONFIGURE fmt
if (NOT TARGET fmt::fmt)
    message("[${PROJECT_NAME}] fmt::fmt not found, fetching ... ")

    FetchContent_Declare(fmt URL https://github.com/fmtlib/fmt/archive/refs/tags/8.1.1.zip)
    FetchContent_Populate(fmt)
    add_subdirectory(${fmt_SOURCE_DIR} ${fmt_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()

### CONFIGURE JSON
if (NOT TARGET nlohmann_json::nlohmann_json)
    message("[${PROJECT_NAME}] nlohmann::json not found, fetching ...")
    set(JSON_MultipleHeaders ON)

    FetchContent_Declare(nlohmann_json URL https://github.com/nlohmann/json/archive/refs/tags/v3.10.5.zip)
    FetchContent_Populate(nlohmann_json)
    add_subdirectory(${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()

### CONFIGURE ASIO
option(perfkit_ASIO_HEADER_ONLY "" ON)

if (perfkit_USE_BUNDLED_ASIO OR perfkit_BUILD_NET_CLIENT OR perfkit_BUILD_WEB)
    find_path(ASIO_INCLUDE_DIR asio/asio.hpp)
    if ("${ASIO_INCLUDE_DIR}" STREQUAL "ASIO_INCLUDE_DIR-NOTFOUND")
        message("[${PROJECT_NAME}] asio not found, fetching ...")
        FetchContent_Declare(asio URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-22-1.zip)
        FetchContent_Populate(asio)

        message("[${PROJECT_NAME}] asio fetched to directory '${asio_SOURCE_DIR}'")

        # Configure HEADER-ONLY version of ASIO
        add_library(perfkit-asio-header-only INTERFACE)
        add_library(perfkit::asio-header-only ALIAS perfkit-asio-header-only)
        target_include_directories(perfkit-asio-header-only INTERFACE ${asio_SOURCE_DIR}/asio/include)
        target_compile_features(perfkit-asio-header-only INTERFACE cxx_std_17)

        # Configure STATIC version of ASIO
        add_library(perfkit-asio-static STATIC)
        add_library(perfkit::asio-static ALIAS perfkit-asio-static)
        target_include_directories(perfkit-asio-static PUBLIC ${asio_SOURCE_DIR}/asio/include)
        target_compile_features(perfkit-asio-static PUBLIC cxx_std_17)
        target_compile_definitions(perfkit-asio-static PUBLIC ASIO_SEPARATE_COMPILATION=1)
        target_sources(perfkit-asio-static PRIVATE ${asio_SOURCE_DIR}/asio/src/asio.cpp)

        set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)

        find_package(OpenSSL QUIET)
        if (OpenSSL_Found)
            message("[${PROJECT_NAME}] asio: OpenSSL found, configuring ssl sources ...")

            target_sources(perfkit-asio-static PRIVATE ${asio_SOURCE_DIR}/asio/src/asio_ssl.cpp)
            target_link_libraries(perfkit-asio-static PRIVATE OpenSSL::SSL)
        endif ()

        if (perfkit_ASIO_HEADER_ONLY)
            message("[${PROJECT_NAME}] asio: using header-only asio as default ...")
            add_library(perfkit::asio ALIAS perfkit-asio-header-only)
        else ()
            message("[${PROJECT_NAME}] asio: using static lib asio as default ...")
            add_library(perfkit::asio ALIAS perfkit-asio-static)
        endif ()
    endif ()
endif ()

### CONFIGURE RANGES
if (NOT TARGET range-v3::range-v3)
    message("[${PROJECT_NAME}] range-v3::range-v3 not found, fetching ...")
    FetchContent_Declare(range-v3 URL https://github.com/ericniebler/range-v3/archive/refs/heads/master.zip)
    FetchContent_Populate(range-v3)
    add_subdirectory(${range-v3_SOURCE_DIR} ${range-v3_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()

### CONFIGURE SPDLOG
if (NOT TARGET spdlog::spdlog)
    message("[${PROJECT_NAME}] spdlog::spdlog not found, fetching ...")
    FetchContent_Declare(spdlog URL https://github.com/gabime/spdlog/archive/refs/tags/v1.10.0.tar.gz)
    FetchContent_Populate(spdlog)
    add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()

### CONFIGURE args
add_subdirectory(taywee-args)

### CONFIGURE cppheaders
if (NOT TARGET kang-sw::cppheaders)
    message("-- [${PROJECT_NAME}]: Configuring cppheaders ...")
    add_subdirectory(cppheaders)
endif ()

project(perfkit.third.cpph)
add_library(${PROJECT_NAME} INTERFACE)
add_library(perfkit::cpph ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} INTERFACE cppheaders cppheaders.refl-extension)

### CONFIGURE crow
if (perfkit_BUILD_WEB AND NOT TARGET Crow::Crow)
    message("[${PROJECT_NAME}] Crow::Crow not found, fetching ...")
    FetchContent_Declare(crow URL https://github.com/CrowCpp/Crow/archive/fde74c4d2e66c76dbe288dfdf080ea1a09d28abd.zip)
    FetchContent_Populate(crow)
    add_subdirectory(${crow_SOURCE_DIR} ${crow_BINARY_DIR} EXCLUDE_FROM_ALL)
endif ()
