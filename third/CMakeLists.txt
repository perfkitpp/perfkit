add_library(
        PicoSHA2 INTERFACE
)

add_library(
        perfkit::picosha2 ALIAS PicoSHA2
)

target_include_directories(
        PicoSHA2 INTERFACE PicoSHA2/
)
