cmake_minimum_required(VERSION 3.14)

add_executable(Lute.CLI)

set_target_properties(Lute.CLI PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    OUTPUT_NAME "lute")

target_sources(Lute.CLI PRIVATE
    "src/main.cpp"
)

target_link_libraries(Lute.CLI PRIVATE Lute.Base Lute.Runtime Lute.Fs)
