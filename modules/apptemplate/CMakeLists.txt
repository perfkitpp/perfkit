project(net-v2-apptemplate)
add_library(${PROJECT_NAME} INTERFACE)
add_library(perfkit::net-v2::template ALIAS ${PROJECT_NAME})

target_sources(${PROJECT_NAME} INTERFACE perfkit-main.cpp)
target_include_directories(${PROJECT_NAME} INTERFACE .)
target_link_libraries(${PROJECT_NAME} INTERFACE perfkit::core)
