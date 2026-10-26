cmake_minimum_required(VERSION 3.25)
project(nutmeg LANGUAGES CXX)

option(NUTMEG_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer (gcc and clang)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NUTMEG_SANITIZERS AND MSVC)
    message(FATAL_ERROR "NUTMEG_SANITIZERS is for clang and gcc")
endif()
if(NUTMEG_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# The simulation must round the same way everywhere. MSVC does not contract
# multiply-adds under its default /fp:precise; clang and gcc do, so tell them
# not to.
if(NOT MSVC)
    add_compile_options(-ffp-contract=off)
endif()

add_library(nutmeg_warnings INTERFACE)
if(MSVC)
    target_compile_options(nutmeg_warnings INTERFACE /W4 /permissive- /utf-8)
    target_compile_definitions(nutmeg_warnings INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(nutmeg_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(sim)
add_subdirectory(tests)
add_subdirectory(schema)
add_subdirectory(physics)
