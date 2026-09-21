/*
    Created on: 8/29/26
    Filename: Renderer.cpp
    Description: bgfx-backed renderer owning the bgfx context and the drawn mesh.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/


#include "Renderer.hpp"

#include <SDL3/SDL.h>
#include <bx/math.h>

#include <cstddef>
#include <cstdint>
#include <iostream>

// Generated at build time by shaderc (see CMakeLists.txt).
#include <generated/shaders/glsl/vs_simple.sc.bin.h>
#include <generated/shaders/glsl/fs_simple.sc.bin.h>
#include <generated/shaders/spirv/vs_simple.sc.bin.h>
#include <generated/shaders/spirv/fs_simple.sc.bin.h>

namespace
{
    constexpr bgfx::ViewId MainView = 0;
    constexpr uint32_t ClearColor = 0x1E1E1EFFu;
    constexpr uint32_t ResetFlags = BGFX_RESET_VSYNC;

    // A triangle is 2D, so depth writes are all the depth handling it needs;
    // back-face culling would only make winding order a source of bugs.
    constexpr uint64_t DrawState = BGFX_STATE_WRITE_RGB
                                 | BGFX_STATE_WRITE_A
                                 | BGFX_STATE_WRITE_Z
                                 | BGFX_STATE_DEPTH_TEST_LESS;

    struct PosColorVertex
    {
        float x, y, z;
        uint32_t abgr;

        static void init(bgfx::VertexLayout& layout)
        {
            layout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
                .end();
        }
    };

    // 0xAABBGGRR
    constexpr uint32_t Red = 0xFF0000FFu;
    constexpr uint32_t Green = 0xFF00FF00u;
    constexpr uint32_t Blue = 0xFFFF0000u;

    // One compiled shader binary, embedded in the executable.
    struct ShaderBlob
    {
        const uint8_t* data;
        uint32_t size;
    };

    // The same shader compiled for every backend we support.
    struct ShaderVariants
    {
        ShaderBlob glsl;
        ShaderBlob spirv;
    };

    template <std::size_t Size>
    constexpr ShaderBlob toBlob(const uint8_t (&data)[Size])
    {
        return ShaderBlob{ data, static_cast<uint32_t>(Size) };
    }

    // bgfx's BGFX_EMBEDDED_SHADER macro is not used on purpose: on Linux it also
    // references the dxbc/essl symbols, which we do not compile.
    bgfx::ShaderHandle createEmbeddedShader(const ShaderVariants& variants, const char* name)
    {
        ShaderBlob blob{ nullptr, 0 };

        switch (bgfx::getRendererType()) {
            case bgfx::RendererType::OpenGL:
                blob = variants.glsl;
                break;
            case bgfx::RendererType::Vulkan:
                blob = variants.spirv;
                break;
            default:
                break;
        }

        if (blob.data == nullptr) {
            std::cerr << "[Renderer] ERROR: no embedded '" << name << "' shader for backend "
                      << bgfx::getRendererName(bgfx::getRendererType()) << "\n";
            return BGFX_INVALID_HANDLE;
        }

        const bgfx::ShaderHandle handle = bgfx::createShader(bgfx::makeRef(blob.data, blob.size));
        if (bgfx::isValid(handle)) {
            bgfx::setName(handle, name);
        }

        return handle;
    }

    // Hands bgfx the native window it must present to.
    bool fillNativeWindow(SDL_Window* window, bgfx::Init& init)
    {
        const SDL_PropertiesID props = SDL_GetWindowProperties(window);
        if (props == 0) {
            std::cerr << "[Renderer] ERROR: SDL_GetWindowProperties failed: " << SDL_GetError() << "\n";
            return false;
        }

#if defined(__linux__)
        const char* videoDriver = SDL_GetCurrentVideoDriver();
        if (videoDriver == nullptr) {
            std::cerr << "[Renderer] ERROR: SDL_GetCurrentVideoDriver returned null\n";
            return false;
        }

        std::cerr << "[Renderer] video driver: " << videoDriver << "\n";

        if (SDL_strcmp(videoDriver, "x11") == 0) {
            void* display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
            const Uint64 handle = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);

            if (display == nullptr || handle == 0) {
                std::cerr << "[Renderer] ERROR: missing X11 window handles\n";
                return false;
            }

            init.platformData.type = bgfx::NativeWindowHandleType::Default;
            init.swapChain.ndt = display;
            init.swapChain.nwh = reinterpret_cast<void*>(static_cast<uintptr_t>(handle));
            return true;
        }

        if (SDL_strcmp(videoDriver, "wayland") == 0) {
            void* display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            void* surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);

            if (display == nullptr || surface == nullptr) {
                std::cerr << "[Renderer] ERROR: missing Wayland window handles\n";
                return false;
            }

            init.platformData.type = bgfx::NativeWindowHandleType::Wayland;
            init.swapChain.ndt = display;
            init.swapChain.nwh = surface;
            return true;
        }

        std::cerr << "[Renderer] ERROR: unsupported Linux video driver: " << videoDriver << "\n";
        return false;
#elif defined(_WIN32)
        void* hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        if (hwnd == nullptr) {
            std::cerr << "[Renderer] ERROR: missing Win32 window handle\n";
            return false;
        }

        init.platformData.type = bgfx::NativeWindowHandleType::Default;
        init.swapChain.nwh = hwnd;
        return true;
#else
        (void)init;
        std::cerr << "[Renderer] ERROR: platform not supported yet\n";
        return false;
#endif
    }
}

bool Renderer::initialize(SDL_Window* window)
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    m_windowWidth = static_cast<uint32_t>(width);
    m_windowHeight = static_cast<uint32_t>(height);
    std::cerr << "[Renderer] back-buffer: " << m_windowWidth << "x" << m_windowHeight << "\n";

    PosColorVertex::init(m_layout);

    // Calling renderFrame() before init() from this same thread keeps bgfx
    // single-threaded, so every bgfx call may come from the SDL thread.
    bgfx::renderFrame();

    bgfx::Init init;
    init.type = bgfx::RendererType::Count;  // let bgfx pick the platform default
    init.reset = ResetFlags;
    init.swapChain.width = m_windowWidth;
    init.swapChain.height = m_windowHeight;

    if (!fillNativeWindow(window, init)) {
        return false;
    }

    if (!bgfx::init(init)) {
        std::cerr << "[Renderer] ERROR: bgfx::init failed\n";
        return false;
    }

    std::cerr << "[Renderer] backend: " << bgfx::getRendererName(bgfx::getRendererType()) << "\n";
    bgfx::setDebug(BGFX_DEBUG_TEXT);

    if (!createProgram()) {
        return false;
    }

    createTriangleMesh();
    std::cerr << "[Renderer] ready\n";

    return true;
}

void Renderer::shutdown()
{
    destroyMesh();

    if (bgfx::isValid(m_program)) {
        bgfx::destroy(m_program);
        m_program = BGFX_INVALID_HANDLE;
    }

    bgfx::shutdown();
}

void Renderer::resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0 || (width == m_windowWidth && height == m_windowHeight)) {
        return;
    }

    m_windowWidth = width;
    m_windowHeight = height;

    // Only the fields set here change; nwh/ndt stay the ones bgfx was given.
    bgfx::SwapChain swapChain;
    swapChain.width = width;
    swapChain.height = height;
    bgfx::reset(ResetFlags, &swapChain);
}

void Renderer::render()
{
    bgfx::setViewClear(MainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, ClearColor, 1.0f, 0);
    bgfx::setViewRect(MainView, 0, 0,
        static_cast<uint16_t>(m_windowWidth),
        static_cast<uint16_t>(m_windowHeight));
    bgfx::touch(MainView);  // clear the view even when nothing is submitted

    float view[16];
    float proj[16];
    bx::mtxIdentity(view);
    bx::mtxOrtho(proj, -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 100.0f, 0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(MainView, view, proj);

    if (bgfx::isValid(m_vbh) && bgfx::isValid(m_program)) {
        bgfx::setVertexBuffer(0, m_vbh);
        bgfx::setState(DrawState);
        bgfx::submit(MainView, m_program);
    }

    bgfx::frame();
}

bool Renderer::createProgram()
{
    const ShaderVariants vertexShader{ toBlob(vs_simple_glsl), toBlob(vs_simple_spv) };
    const ShaderVariants fragmentShader{ toBlob(fs_simple_glsl), toBlob(fs_simple_spv) };

    const bgfx::ShaderHandle vsh = createEmbeddedShader(vertexShader, "vs_simple");
    const bgfx::ShaderHandle fsh = createEmbeddedShader(fragmentShader, "fs_simple");

    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        return false;
    }

    m_program = bgfx::createProgram(vsh, fsh, true /* destroy shaders with program */);
    if (!bgfx::isValid(m_program)) {
        std::cerr << "[Renderer] ERROR: bgfx::createProgram failed\n";
        return false;
    }

    return true;
}

void Renderer::createTriangleMesh()
{
    static const PosColorVertex vertices[] = {
        {  0.0f,  0.5f, 0.0f, Red   },
        {  0.5f, -0.5f, 0.0f, Green },
        { -0.5f, -0.5f, 0.0f, Blue  },
    };

    // copy(), not makeRef(): bgfx reads vertex data on a later frame, so it must
    // own it rather than point at memory the caller may free.
    m_vbh = bgfx::createVertexBuffer(bgfx::copy(vertices, sizeof(vertices)), m_layout);
}

void Renderer::destroyMesh()
{
    if (bgfx::isValid(m_vbh)) {
        bgfx::destroy(m_vbh);
        m_vbh = BGFX_INVALID_HANDLE;
    }
}
