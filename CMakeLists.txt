cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(mach_ipc)

# Ensure ninja shows color diagnostics
add_compile_options(-fdiagnostics-color=always)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Example, always build in debug
add_definitions(-O0 -g)

add_subdirectory(mach_messages_one)
