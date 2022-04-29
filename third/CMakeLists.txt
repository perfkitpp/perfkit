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

### Try find prebuild packages
find_package(fmt QUIET)
find_package(spdlog QUIET)

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
if (perfkit_USE_BUNDLED_ASIO OR perfkit_BUILD_NET_CLIENT)
    find_path(ASIO_INCLUDE_DIR asio/asio.hpp)
    if ("${ASIO_INCLUDE_DIR}" STREQUAL "ASIO_INCLUDE_DIR-NOTFOUND")
        message("[${PROJECT_NAME}] asio not found, fetching ...")
        FetchContent_Declare(asio URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-22-1.zip)
        FetchContent_Populate(asio)

        message("[${PROJECT_NAME}] asio fetched to directory '${asio_SOURCE_DIR}'")
        add_library(asio INTERFACE)
        target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
        target_compile_features(asio INTERFACE cxx_std_17)

        add_library(perfkit::asio ALIAS asio)
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

### CONFIGURE cppheaders
add_subdirectory(cppheaders)
add_library(perfkit::cpph ALIAS cppheaders)