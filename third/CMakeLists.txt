add_library(
        PicoSHA2 INTERFACE
)

add_library(
        perfkit::picosha2 ALIAS PicoSHA2
)

target_include_directories(
        PicoSHA2 INTERFACE PicoSHA2/
)

if(NOT TARGET nlohmann::json)
    add_library(json INTERFACE)
    add_library(nlohmann::json ALIAS json)
    target_compile_features(json INTERFACE cxx_std_17)
    target_include_directories(json INTERFACE json)
endif()
