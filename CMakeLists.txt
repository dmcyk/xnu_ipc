cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(mach_ipc)

# Ensure ninja shows color diagnostics
add_compile_options(-fdiagnostics-color=always)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Example, always build in debug
add_definitions(-O0 -g)

add_subdirectory(01_mach_messages)
add_subdirectory(02_mach_bidirectional)
add_subdirectory(03_mach_ool_intro)
