cmake_minimum_required(VERSION 3.14)

add_library(Lute.Net STATIC)

set_target_properties(Lute.Net PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

target_sources(Lute.Net PRIVATE
    "src/net.cpp"
)
target_include_directories(Lute.Net PUBLIC include/)
target_link_libraries(Lute.Net PRIVATE Lute.Base Lute.Runtime Luau.Common uWS uSockets)
