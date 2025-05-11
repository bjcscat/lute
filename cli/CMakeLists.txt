cmake_minimum_required(VERSION 3.14)

add_executable(Lute.CLI)

set(CLI_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/cli.luau")
set(CLI_HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated/cli_source.h")

embed_file(
    OUTPUT_HEADER ${CLI_HEADER_FILE}
    VARIABLE_NAME "LUTE_CLI_FILE"
    INPUT_FILES ${CLI_FILE}
)

set_target_properties(Lute.CLI PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    OUTPUT_NAME "lute")

target_sources(Lute.CLI PRIVATE
    "src/main.cpp"
    ${CLI_HEADER_FILE}
)

target_include_directories(Lute.CLI PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_link_libraries(Lute.CLI PRIVATE Lute.Base Lute.Runtime Lute.Fs Lute.Std Lute.Time Lute.Net)
