/*
    Created on: 9/22/26
    Filename: IRenderer.hpp
    Description: Backend-agnostic 2D rendering interface.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#ifndef RENDERER_WRAPPER_IRENDERER_HPP
#define RENDERER_WRAPPER_IRENDERER_HPP

#include "Renderer/RendererTypes.hpp"

#include <cstdint>

namespace gfx {

    class IRenderer {
        public:
            virtual ~IRenderer() = default;

            IRenderer(const IRenderer&) = delete;
            IRenderer& operator=(const IRenderer&) = delete;

            // Back-buffer size in pixels; does not move the camera.
            virtual void resize(uint32_t width, uint32_t height) = 0;

            virtual void beginFrame(Color clear) = 0;
            virtual void endFrame() = 0;

            // The camera set last before endFrame() applies to every World draw of the frame.
            virtual void setCamera(const Camera2D& camera) = 0;

            // Pixels are tightly packed RGBA8 and are copied; the caller may free them on return.
            virtual TextureHandle createTexture(const uint8_t* rgba, uint32_t width, uint32_t height) = 0;
            virtual void destroyTexture(TextureHandle texture) = 0;
            virtual Vec2 textureSize(TextureHandle texture) const = 0;

            virtual void drawRect(const RectDraw& draw) = 0;
            virtual void drawSprite(const SpriteDraw& draw) = 0;

            virtual const char* backendName() const = 0;

        protected:
            IRenderer() = default;
    };

}

#endif //RENDERER_WRAPPER_IRENDERER_HPP
