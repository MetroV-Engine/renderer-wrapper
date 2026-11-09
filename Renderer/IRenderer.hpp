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

#include "Renderer/Font/Font.hpp"
#include "Renderer/RendererTypes.hpp"
#include "Renderer/ShapeBuilder/ShapeBuilder.hpp"

#include <cstdint>
#include <iostream>

namespace gfx {

    class IRenderer {
        public:
            virtual ~IRenderer() = default;

            IRenderer(const IRenderer&) = delete;
            IRenderer& operator=(const IRenderer&) = delete;

            // Back-buffer size in pixels; does not move the camera.
            virtual void resize(uint32_t width, uint32_t height) = 0;
            // TODO: rotate

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
            // The spans are only read during the call; backends copy what they queue.
            virtual void drawMesh(const MeshDraw& draw) = 0;

            void drawLine(const LineDraw& draw)
            {
                m_scratch.clear();
                appendLine(m_scratch, draw);
                submitScratch(TextureHandle::Invalid, draw.layer, draw.space);
            }

            void drawTriangle(const TriangleDraw& draw)
            {
                m_scratch.clear();
                appendTriangle(m_scratch, draw);
                submitScratch(TextureHandle::Invalid, draw.layer, draw.space);
            }

            void drawQuad(const QuadDraw& draw)
            {
                m_scratch.clear();
                appendQuad(m_scratch, draw);
                submitScratch(TextureHandle::Invalid, draw.layer, draw.space);
            }

            void drawCircle(const CircleDraw& draw)
            {
                m_scratch.clear();
                appendCircle(m_scratch, draw);
                submitScratch(TextureHandle::Invalid, draw.layer, draw.space);
            }

            void drawPolygon(const PolygonDraw& draw)
            {
                if (draw.points.size() < 3) {
                    return;
                }

                m_scratch.clear();
                if (!appendPolygon(m_scratch, draw)) {
                    if (!m_warnedPolygon) {
                        std::cerr << "[IRenderer] WARN: drawPolygon could not triangulate its points; ignoring\n";
                        m_warnedPolygon = true;
                    }
                    return;
                }

                submitScratch(TextureHandle::Invalid, draw.layer, draw.space);
            }

            void drawText(const TextDraw& draw)
            {
                if (draw.font == nullptr) {
                    if (!m_warnedNullFont) {
                        std::cerr << "[IRenderer] WARN: drawText called with a null font; ignoring\n";
                        m_warnedNullFont = true;
                    }
                    return;
                }

                m_scratch.clear();
                draw.font->appendText(m_scratch, draw);
                submitScratch(draw.font->texture(), draw.layer, draw.space);
            }

            virtual const char* backendName() const = 0;

        protected:
            IRenderer() = default;

        private:
            void submitScratch(const TextureHandle texture, const Layer layer, const Space space)
            {
                if (m_scratch.vertices.empty()) {
                    return;
                }

                drawMesh(MeshDraw{ m_scratch.vertices, m_scratch.indices, texture, layer, space });
            }

            TriangleMesh m_scratch;
            bool m_warnedPolygon = false;
            bool m_warnedNullFont = false;
    };

}

#endif //RENDERER_WRAPPER_IRENDERER_HPP
