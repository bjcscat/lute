cmake_minimum_required(VERSION 3.14)

add_library(Lute.Time STATIC)

set_target_properties(Lute.Time PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

target_sources(Lute.Time PRIVATE
    "include/lute/time.h"

    "src/time.cpp"
)

target_include_directories(Lute.Time PUBLIC include/)
target_link_libraries(Lute.Time PRIVATE Lute.Base)
