cmake_minimum_required(VERSION 3.24)
project(renderer_wrapper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- bgfx / bx / bimg via bgfx.cmake ---
include(FetchContent)

# CLion lists every target as a run configuration, so build only what we consume.
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

# bgfx.cmake force-enables example-common alongside its tools; nothing here links it.
if (TARGET example-common)
    set_target_properties(example-common PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

find_package(SDL3 CONFIG REQUIRED)

# --- Shader compilation ------------------------------------------------------
# shaderc emits C headers at build time: nothing to ship, no working-directory guessing.

# BGFX_SHADER_INCLUDE_PATH only exists in bgfx.cmake's installed config, not under FetchContent.
set(BGFX_SHADER_INCLUDE_PATH ${bgfx_SOURCE_DIR}/bgfx/src)
include(${bgfx_SOURCE_DIR}/cmake/bgfxToolUtils.cmake)

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/shaders)
# Both are embedded; Renderer picks the one matching the backend bgfx selected at runtime.
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

# Warnings are per target so fetched dependencies (bgfx, googletest) keep their own flags.
function(renderer_wrapper_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# --- Backend-free core -------------------------------------------------------
# No SDL, no bgfx: the playground and the headless tests both link it.
add_library(renderer_core STATIC
        Renderer/RendererTypes.hpp
        Renderer/IRenderer.hpp
        Renderer/Transform/Transform.cpp
        Renderer/Transform/Transform.hpp
        Renderer/NullRenderer/NullRenderer.cpp
        Renderer/NullRenderer/NullRenderer.hpp
        Renderer/SpriteAnimation/SpriteAnimation.cpp
        Renderer/SpriteAnimation/SpriteAnimation.hpp
)
target_include_directories(renderer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
renderer_wrapper_warnings(renderer_core)

# --- Executable --------------------------------------------------------------
add_executable(renderer_wrapper
        main.cpp
        Renderer/RendererFactory/RendererFactory.cpp
        Renderer/RendererFactory/RendererFactory.hpp
        Renderer/SdlWindowBridge/SdlWindowBridge.cpp
        Renderer/SdlWindowBridge/SdlWindowBridge.hpp
        Renderer/SdlRenderer/SdlRenderer.cpp
        Renderer/SdlRenderer/SdlRenderer.hpp
        Renderer/Renderer/Renderer.cpp
        Renderer/Renderer/Renderer.hpp
        Tools/ImageLoader/ImageLoader.cpp
        Tools/ImageLoader/ImageLoader.hpp
        Playground/Player/Player.cpp
        Playground/Player/Player.hpp
        Playground/ModeToggle/ModeToggle.cpp
        Playground/ModeToggle/ModeToggle.hpp
        Playground/Showcase/Showcase.cpp
        Playground/Showcase/Showcase.hpp
        # As sources, these run shaderc before compiling and again on every .sc edit.
        ${VERTEX_SHADER_HEADERS}
        ${FRAGMENT_SHADER_HEADERS}
)

target_include_directories(renderer_wrapper PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}  # generated/shaders/<profile>/*.bin.h
)

target_link_libraries(renderer_wrapper PRIVATE
        renderer_core
        SDL3::SDL3
        bgfx
        bx
        bimg
)

# Assets live next to the binary: the code resolves them from SDL_GetBasePath(), never the cwd.
add_custom_command(TARGET renderer_wrapper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/assets
                $<TARGET_FILE_DIR:renderer_wrapper>/assets
)

renderer_wrapper_warnings(renderer_wrapper)

# --- Unit tests --------------------------------------------------------------
# Headless: only renderer_core, so they run without a window or GPU.
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.18.0
        GIT_SHALLOW    TRUE
)
set(INSTALL_GTEST OFF)
set(BUILD_GMOCK OFF)
set(gtest_force_shared_crt ON)    # MSVC: link the same C runtime as our targets
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(renderer_tests
        Tests/TransformTests.cpp
        Tests/SpriteAnimationTests.cpp
        Tests/NullRendererTests.cpp
)
target_link_libraries(renderer_tests PRIVATE
        renderer_core
        GTest::gtest_main
)
renderer_wrapper_warnings(renderer_tests)

include(GoogleTest)
gtest_discover_tests(renderer_tests)
