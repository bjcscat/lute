cmake_minimum_required(VERSION 3.14)

add_library(Lute.Std INTERFACE)

file(GLOB LUAU_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.luau
)

set(GENERATED_HEADER ${CMAKE_CURRENT_BINARY_DIR}/generated/lute_std_source.h)
generate_file_map_header(
    OUTPUT_HEADER ${GENERATED_HEADER}
    MAP_NAME "STD_LIBS"
    INPUT_FILES ${LUAU_SOURCE_FILES}
)

target_include_directories(Lute.Std INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/generated)
