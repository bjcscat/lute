cmake_minimum_required(VERSION 3.14)

add_library(Lute.Runtime STATIC)

set_target_properties(Lute.Runtime PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

target_sources(Lute.Runtime PRIVATE
    "src/Runtime.cpp"
    "src/Scheduler.cpp"
    # "src/RuntimeLib.cpp"
    "src/Require.cpp"
)
target_include_directories(Lute.Runtime PUBLIC include/)
target_link_libraries(Lute.Runtime PRIVATE Lute.Base Luau.Compiler Luau.CodeGen Luau.Require Luau.Config)
