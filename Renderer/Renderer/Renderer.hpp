/*
    Created on: 8/29/26
    Filename: Renderer.hpp
    Description: bgfx-backed renderer owning the bgfx context and the drawn mesh.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/


#ifndef RENDERER_WRAPPER_RENDERER_HPP
#define RENDERER_WRAPPER_RENDERER_HPP

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <cstdint>

class Renderer {
    public:
        bool initialize(SDL_Window* window);
        void shutdown();

        // Back-buffer size in pixels; call on every window pixel-size change.
        void resize(uint32_t width, uint32_t height);
        void render();

    private:
        bool createProgram();
        void createTriangleMesh();
        void destroyMesh();

        bgfx::VertexLayout m_layout{};
        bgfx::VertexBufferHandle m_vbh{ bgfx::kInvalidHandle };
        bgfx::ProgramHandle m_program{ bgfx::kInvalidHandle };

        uint32_t m_windowWidth = 1280;
        uint32_t m_windowHeight = 720;
};


#endif //RENDERER_WRAPPER_RENDERER_HPP
