cmake_minimum_required(VERSION 3.14)

add_library(Lute.Base INTERFACE)

target_include_directories(Lute.Base INTERFACE "include/")

target_link_libraries(Lute.Base INTERFACE Luau.VM uv_a)
