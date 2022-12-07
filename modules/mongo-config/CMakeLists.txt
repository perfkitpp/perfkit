find_package(mongoc-1.0 CONFIG QUIET)
if (NOT mongoc-1.0_FOUND)
    return()
endif ()

project(perfkit-mongo-config)
add_library(${PROJECT_NAME} STATIC)
add_library(perfkit::utils::mongo-config ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)

if (MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE mongo::mongoc_shared)
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE mongo::mongoc_static)
endif ()

target_sources(${PROJECT_NAME} PRIVATE perfkit-mongo-config-io.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC .)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)


project(perfkit-mongo-config-test)
add_executable(${PROJECT_NAME} perfkit-mongo-config-io-test.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE perfkit-mongo-config)

