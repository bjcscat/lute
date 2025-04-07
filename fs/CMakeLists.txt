cmake_minimum_required(VERSION 3.14)

add_library(Lute.Fs STATIC)

set_target_properties(Lute.Fs PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

target_sources(Lute.Fs PRIVATE
    "include/lute/fs.h"

    "src/fs.cpp"
)
target_include_directories(Lute.Fs PUBLIC include/)
target_link_libraries(Lute.Fs PRIVATE Lute.Base Lute.Runtime)
