project(perfkit-bundled-taywee-args)
add_library(${PROJECT_NAME} INTERFACE)
add_library(perfkit::third::args ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} INTERFACE include)
