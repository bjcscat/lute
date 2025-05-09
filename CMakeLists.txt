cmake_minimum_required(VERSION 3.14)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

# Make sure 'set' for variables is respected by options in nested files
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_CXX_CLANG_TIDY clang-tidy)

# ---- Project ----

project(
    Lute
    VERSION 0.1
    LANGUAGES CXX C
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(
        FATAL_ERROR
            "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()

add_compile_options(-fsanitize=undefined,address -g)
add_link_options(-fsanitize=undefined,address -g)

include(cmake/CPM.cmake)
include(Dependencies.cmake)

include(cmake/generate_file.cmake)
include(cmake/tools.cmake)

add_subdirectory(base)
add_subdirectory(std)
add_subdirectory(runtime)

add_subdirectory(fs)
add_subdirectory(time)

add_subdirectory(cli)
