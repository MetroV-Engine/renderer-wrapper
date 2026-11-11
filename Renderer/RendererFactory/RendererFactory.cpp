/*
    Created on: 9/22/26
    Filename: RendererFactory.cpp
    Description: Creates a renderer for a backend; the one line a caller changes to switch.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/RendererFactory/RendererFactory.hpp"

#include "Renderer/BgfxRenderer/BgfxRenderer.hpp"
#include "Renderer/NullRenderer/NullRenderer.hpp"
#include "Renderer/SdlRenderer/SdlRenderer.hpp"

namespace gfx {

    std::unique_ptr<IRenderer> makeRenderer(const Backend backend, SDL_Window* window,
                                            const uint32_t width, const uint32_t height)
    {
        switch (backend) {
            case Backend::Sdl3Cpu:
                return SdlRenderer::create(window, width, height);

            case Backend::Null:
                return std::make_unique<NullRenderer>(width, height);

            case Backend::Bgfx:
                return BgfxRenderer::create(window, width, height);
        }

        return nullptr;
    }

}
