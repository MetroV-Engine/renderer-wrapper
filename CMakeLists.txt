cmake_minimum_required(VERSION 3.24)
project(renderer_wrapper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- bgfx / bx / bimg via bgfx.cmake ---
include(FetchContent)

# bgfx.cmake builds its examples and every tool by default, and CLion turns each
# resulting target into a run configuration. Keep only what we consume, so
# renderer_wrapper is the one runnable target in the project.
set(BGFX_BUILD_EXAMPLES OFF)      # dozens of example-* executables
set(BGFX_BUILD_TESTS OFF)
set(BGFX_INSTALL OFF)             # no install target to list
set(BGFX_CUSTOM_TARGETS OFF)      # "tools"/"examples" convenience targets
set(BGFX_BUILD_TOOLS_GEOMETRY OFF)
set(BGFX_BUILD_TOOLS_TEXTURE OFF) # texturev is a runnable GUI app
set(BGFX_BUILD_TOOLS_BIN2C OFF)   # shaderc --bin2c covers our use
# BGFX_BUILD_TOOLS stays ON: bgfx::shaderc compiles our shaders.

FetchContent_Declare(
        bgfx
        GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
        GIT_TAG        v1.161.9495-578  # pinned: master moves and breaks the init API
        GIT_SUBMODULES_RECURSE TRUE
)
FetchContent_MakeAvailable(bgfx)
# ^ creates the bgfx, bx, bimg targets and the bgfx::shaderc tool.

# bgfx.cmake force-enables example-common whenever its tools are built, so the
# option above cannot switch it off. Nothing here links it (it is dear-imgui,
# nanovg and friends), so keep it out of the default build.
if (TARGET example-common)
    set_target_properties(example-common PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

find_package(SDL3 CONFIG REQUIRED)

# --- Shader compilation ------------------------------------------------------
# shaderc turns shaders/*.sc into C headers at build time, so the executable is
# self-contained: no shader binaries to ship, no working-directory assumptions.
#
# bgfx.cmake only defines BGFX_SHADER_INCLUDE_PATH in its *installed* config
# file, so with FetchContent we point it at the sources ourselves.
set(BGFX_SHADER_INCLUDE_PATH ${bgfx_SOURCE_DIR}/bgfx/src)
include(${bgfx_SOURCE_DIR}/cmake/bgfxToolUtils.cmake)

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/shaders)
# 430 -> GLSL (OpenGL backend), spirv -> SPIR-V (Vulkan backend).
# Both are embedded; Renderer picks the one matching the backend bgfx selected.
set(SHADER_PROFILES 430 spirv)

bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_simple.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        PROFILES ${SHADER_PROFILES}
        AS_HEADERS
        OUT_FILES_VAR VERTEX_SHADER_HEADERS
)

bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_simple.sc
        VARYING_DEF ${CMAKE_CURRENT_SOURCE_DIR}/shaders/varying.def.sc
        OUTPUT_DIR ${SHADER_OUTPUT_DIR}
        PROFILES ${SHADER_PROFILES}
        AS_HEADERS
        OUT_FILES_VAR FRAGMENT_SHADER_HEADERS
)

# --- Executable --------------------------------------------------------------
add_executable(renderer_wrapper
        main.cpp
        Renderer/Renderer.cpp
        Renderer/Renderer.hpp
        # Listing the generated headers as sources makes the build run shaderc
        # before compiling, and re-run it whenever a .sc file changes.
        ${VERTEX_SHADER_HEADERS}
        ${FRAGMENT_SHADER_HEADERS}
)

target_include_directories(renderer_wrapper PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}  # generated/shaders/<profile>/*.bin.h
)

target_link_libraries(renderer_wrapper PRIVATE
        SDL3::SDL3
        bgfx
        bx
        bimg
)

if (MSVC)
    target_compile_options(renderer_wrapper PRIVATE /W4)
else()
    target_compile_options(renderer_wrapper PRIVATE -Wall -Wextra -Wpedantic)
endif()
