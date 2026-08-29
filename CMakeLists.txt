cmake_minimum_required(VERSION 3.24)
project(renderer_wrapper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(renderer_wrapper
        main.cpp
)

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(renderer_wrapper PRIVATE SDL3::SDL3)

if (MSVC)
    target_compile_options(renderer_wrapper PRIVATE /W4)
else()
    target_compile_options(renderer_wrapper PRIVATE -Wall -Wextra -Wpedantic)
endif()